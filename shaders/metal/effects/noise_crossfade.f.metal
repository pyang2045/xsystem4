// Generated from shaders/effects/noise_crossfade.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 12
// @u resolution 0 vec2 1 0
// @u progress 8 float 1 0
// @tex old 0 0
// @tex tex 1 1
// @in tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float2 resolution;
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

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _15 [[buffer(0)]], texture2d<float> old [[texture(0)]], texture2d<float> tex [[texture(1)]], sampler oldSmplr [[sampler(0)]], sampler texSmplr [[sampler(1)]])
{
    main0_out out = {};
    float rand = fract(sin(dot(float3(in.tex_coord, _15.progress), float3(12.98980045318603515625, 78.233001708984375, 81.24629974365234375))) * 43758.546875);
    float3 col;
    if (_15.progress < 0.5)
    {
        col = mix(old.sample(oldSmplr, in.tex_coord).xyz, float3(rand), float3(_15.progress * 2.0));
    }
    else
    {
        col = mix(tex.sample(texSmplr, in.tex_coord).xyz, float3(rand), float3((1.0 - _15.progress) * 2.0));
    }
    out.frag_color = float4(col, 1.0);
    return out;
}

