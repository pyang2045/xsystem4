// Generated from shaders/fill.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 16
// @u color 0 vec4 1 0
// @in tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float4 color;
};

struct main0_out
{
    float4 frag_color [[color(0)]];
};

fragment main0_out main0(constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _12 [[buffer(0)]])
{
    main0_out out = {};
    out.frag_color = _12.color;
    return out;
}

