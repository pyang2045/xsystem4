#!/usr/bin/env python3
"""Translate the 2D GLSL shaders to Metal Shading Language for the Metal backend.

  shaders/metal/glsl2msl.py        (run from anywhere; needs glslang and spirv-cross)

Each shaders/X.glsl (and shaders/effects/X.glsl) becomes shaders/metal/X.metal: GLSL ->
SPIR-V (glslang, Vulkan relaxed rules: loose uniforms go to one default block) -> MSL
(spirv-cross). The 3D shaders (reign*, dungeon*) are skipped: the Metal backend has no 3D.

The output starts with comment lines that src/metal/gl_metal.m reads in place of GL
program introspection:
  // @ubo BUFFER SIZE                      the default uniform block
  // @u NAME OFFSET TYPE COUNT STRIDE       a member of it
  // @tex NAME TEXTURE SAMPLER             a sampler2D
  // @attr NAME LOCATION                   a vertex input
  // @in NAME LOCATION / @out NAME LOCATION   varyings, checked when linking
Vertex shaders also take `constant float4 &xs4_flip [[buffer(30)]]`: gl_Position.y is
multiplied by xs4_flip.x, so that offscreen targets keep GL's row order.

Every output is then compiled with the system's Metal compiler (through swift), the way
the engine compiles it at run time: GLSL names that are reserved in Metal, such as `new`,
fail here rather than when a scene first uses the shader.
"""
import json, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SHADERS = os.path.dirname(HERE)
SKIP = ('reign', 'dungeon')
FLIP_BUFFER = 30


def run(*args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'{args[0]} failed:\n{r.stdout}{r.stderr}')
    return r.stdout


def translate(src, dst, tmp):
    stage = 'vert' if src.endswith('.v.glsl') else 'frag'
    glsl = os.path.join(tmp, 'in.' + stage)
    spv = os.path.join(tmp, 'out.spv')
    with open(src) as f:
        body = f.read()
    with open(glsl, 'w') as f:
        f.write('#version 450\n' + body)
    run('glslang', '-V', '-R', '--aml', '--amb', '-S', stage, '-o', spv, glsl)
    msl = run('spirv-cross', spv, '--msl', '--msl-version', '20100', '--fixup-clipspace')
    refl = json.loads(run('spirv-cross', spv, '--reflect'))

    head = []
    sig = re.search(r'^(vertex|fragment) \w+ main0\((.*)\)$', msl, re.M)
    if not sig:
        sys.exit(f'{src}: no entry point in the MSL output')
    params = sig.group(2)
    types = refl.get('types', {})
    for ubo in refl.get('ubos', []):
        m = re.search(r'& \w+ \[\[buffer\((\d+)\)\]\]', params)
        head.append(f'@ubo {m.group(1)} {ubo["block_size"]}')
        for mem in types[ubo['type']]['members']:
            count = mem.get('array', [1])[0]
            stride = mem.get('array_stride', 0)
            head.append(f'@u {mem["name"]} {mem["offset"]} {mem["type"]} {count} {stride}')
    for t in refl.get('textures', []):
        name = t['name']
        tx = re.search(r'\b%s \[\[texture\((\d+)\)\]\]' % name, params)
        sm = re.search(r'\b%sSmplr \[\[sampler\((\d+)\)\]\]' % name, params)
        head.append(f'@tex {name} {tx.group(1)} {sm.group(1)}')
    if stage == 'vert':
        for i in refl.get('inputs', []):
            head.append(f'@attr {i["name"]} {i["location"]}')
        for o in refl.get('outputs', []):
            head.append(f'@out {o["name"]} {o["location"]}')
        # the flip factor for gl_Position.y
        msl = msl.replace(sig.group(0), sig.group(0)[:-1]
                          + f', constant float4& xs4_flip [[buffer({FLIP_BUFFER})]])', 1)
        msl, n = re.subn(r'^(\s*)return out;$', r'\1out.gl_Position.y *= xs4_flip.x;\n\1return out;',
                         msl, flags=re.M)
        if n != 1:
            sys.exit(f'{src}: expected one return in the vertex shader')
    else:
        for i in refl.get('inputs', []):
            head.append(f'@in {i["name"]} {i["location"]}')
    rel = os.path.relpath(src, SHADERS)
    out = [f'// Generated from shaders/{rel} by shaders/metal/glsl2msl.py; do not edit.']
    out += ['// ' + h for h in head]
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst + '.new', 'w') as f:
        f.write('\n'.join(out) + '\n' + msl)
    os.replace(dst + '.new', dst)


# Compiles the files given on the command line as gl_metal.m does (MSL 2.1, safe math).
CHECK_SWIFT = r'''
import Metal
import Foundation
let dev = MTLCreateSystemDefaultDevice()!
let opts = MTLCompileOptions()
opts.languageVersion = .version2_1
opts.mathMode = .safe
var bad = 0
for p in CommandLine.arguments.dropFirst() {
    do { _ = try dev.makeLibrary(source: try String(contentsOfFile: p, encoding: .utf8), options: opts) }
    catch { bad += 1; print("\(p): \(error)") }
}
exit(bad == 0 ? 0 : 1)
'''


def check(paths, tmp):
    script = os.path.join(tmp, 'check.swift')
    with open(script, 'w') as f:
        f.write(CHECK_SWIFT)
    if subprocess.run(['xcrun', 'swift', script] + paths).returncode != 0:
        sys.exit('Metal rejected the shaders above')


def main():
    n = 0
    outputs = []
    with tempfile.TemporaryDirectory() as tmp:
        for sub in ('', 'effects'):
            d = os.path.join(SHADERS, sub)
            for name in sorted(os.listdir(d)):
                if not name.endswith('.glsl') or name.startswith(SKIP):
                    continue
                dst = os.path.join(HERE, sub, name[:-len('.glsl')] + '.metal')
                translate(os.path.join(d, name), dst, tmp)
                outputs.append(dst)
                n += 1
        check(outputs, tmp)
    print(f'{n} shaders, all compile')


if __name__ == '__main__':
    main()
