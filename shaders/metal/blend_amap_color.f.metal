// Generated from shaders/blend_amap_color.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 16
// @u color 0 vec4 1 0
// @tex tex 0 0
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

struct main0_in
{
    float2 tex_coord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _24 [[buffer(0)]], texture2d<float> tex [[texture(0)]], sampler texSmplr [[sampler(0)]])
{
    main0_out out = {};
    float4 texel = tex.sample(texSmplr, in.tex_coord);
    out.frag_color = float4(_24.color.xyz, texel.w * _24.color.w);
    return out;
}

