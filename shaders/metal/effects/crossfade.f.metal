// Generated from shaders/effects/crossfade.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 4
// @u progress 0 float 1 0
// @tex old 0 0
// @tex tex 1 1
// @in tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float progress;
};

struct main0_out
{
    float4 frag_color [[color(0)]];
};

struct main0_in
{
    float2 tex_coord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _27 [[buffer(0)]], texture2d<float> old [[texture(0)]], texture2d<float> tex [[texture(1)]], sampler oldSmplr [[sampler(0)]], sampler texSmplr [[sampler(1)]])
{
    main0_out out = {};
    float3 col = mix(old.sample(oldSmplr, in.tex_coord), tex.sample(texSmplr, in.tex_coord), float4(_27.progress)).xyz;
    out.frag_color = float4(col, 1.0);
    return out;
}

