// Generated from shaders/effects/blur_crossfade.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 12
// @u resolution 0 vec2 1 0
// @u progress 8 float 1 0
// @tex old 0 0
// @tex tex 1 1
// @in tex_coord 0
#pragma clang diagnostic ignored "-Wmissing-prototypes"

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

static inline __attribute__((always_inline))
float3 get_pixel(thread const float2& xy, texture2d<float> old, sampler oldSmplr, texture2d<float> tex, sampler texSmplr, constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _30)
{
    return mix(old.sample(oldSmplr, xy).xyz, tex.sample(texSmplr, xy).xyz, float3(_30.progress));
}

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _30 [[buffer(0)]], texture2d<float> old [[texture(0)]], texture2d<float> tex [[texture(1)]], sampler oldSmplr [[sampler(0)]], sampler texSmplr [[sampler(1)]])
{
    main0_out out = {};
    float2 p = in.tex_coord * _30.resolution;
    float blur_size = (1.0 - abs((_30.progress * 2.0) - 1.0)) * 100.0;
    float blur_half = blur_size / 2.0;
    float3 gauss;
    gauss.x = 0.0079788453876972198486328125;
    gauss.y = 0.999800026416778564453125;
    gauss.z = gauss.y * gauss.y;
    float3 avg = float3(0.0);
    float coefficient_sum = 0.0;
    float2 param = p / _30.resolution;
    avg += (get_pixel(param, old, oldSmplr, tex, texSmplr, _30) * gauss.x);
    coefficient_sum += gauss.x;
    float3 _99 = gauss;
    float3 _101 = gauss;
    float2 _103 = _101.xy * _99.yz;
    gauss.x = _103.x;
    gauss.y = _103.y;
    float2 tex_offset = float2(0.25, 0.75);
    for (float i = 1.0; i <= blur_half; i += 2.0)
    {
        float2 param_1 = (p - (tex_offset * i)) / _30.resolution;
        avg += (get_pixel(param_1, old, oldSmplr, tex, texSmplr, _30) * gauss.x);
        float2 param_2 = (p + (tex_offset * i)) / _30.resolution;
        avg += (get_pixel(param_2, old, oldSmplr, tex, texSmplr, _30) * gauss.x);
        coefficient_sum += (2.0 * gauss.x);
        float3 _157 = gauss;
        float3 _159 = gauss;
        float2 _161 = _159.xy * _157.yz;
        gauss.x = _161.x;
        gauss.y = _161.y;
    }
    tex_offset = float2(0.75, 0.25);
    for (float i_1 = 1.0; i_1 <= blur_half; i_1 += 2.0)
    {
        float2 param_3 = (p - (tex_offset * i_1)) / _30.resolution;
        avg += (get_pixel(param_3, old, oldSmplr, tex, texSmplr, _30) * gauss.x);
        float2 param_4 = (p + (tex_offset * i_1)) / _30.resolution;
        avg += (get_pixel(param_4, old, oldSmplr, tex, texSmplr, _30) * gauss.x);
        coefficient_sum += (2.0 * gauss.x);
        float3 _213 = gauss;
        float3 _215 = gauss;
        float2 _217 = _215.xy * _213.yz;
        gauss.x = _217.x;
        gauss.y = _217.y;
    }
    out.frag_color = float4(avg / float3(coefficient_sum), 1.0);
    return out;
}

