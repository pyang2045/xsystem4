// Generated from shaders/sprite.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 28
// @u blend_rate 0 float 1 0
// @u multiply_color 16 vec3 1 0
// @tex tex 0 0
// @in tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float blend_rate;
    float3 multiply_color;
};

struct main0_out
{
    float4 frag_color [[color(0)]];
};

struct main0_in
{
    float2 tex_coord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _23 [[buffer(0)]], texture2d<float> tex [[texture(0)]], sampler texSmplr [[sampler(0)]])
{
    main0_out out = {};
    out.frag_color = tex.sample(texSmplr, in.tex_coord);
    float4 _29 = out.frag_color;
    float3 _31 = _29.xyz * _23.multiply_color;
    out.frag_color.x = _31.x;
    out.frag_color.y = _31.y;
    out.frag_color.z = _31.z;
    out.frag_color.w *= _23.blend_rate;
    return out;
}

