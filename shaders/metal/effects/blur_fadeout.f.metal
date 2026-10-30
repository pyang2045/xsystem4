// Generated from shaders/effects/blur_fadeout.f.glsl by shaders/metal/glsl2msl.py; do not edit.
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
float3 get_pixel(thread const float2& xy, texture2d<float> old, sampler oldSmplr)
{
    return old.sample(oldSmplr, xy).xyz;
}

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _31 [[buffer(0)]], texture2d<float> old [[texture(0)]], texture2d<float> tex [[texture(1)]], sampler oldSmplr [[sampler(0)]], sampler texSmplr [[sampler(1)]])
{
    main0_out out = {};
    float2 p = in.tex_coord * _31.resolution;
    float blur_size = _31.progress * 100.0;
    float blur_half = blur_size / 2.0;
    float3 gauss;
    gauss.x = 0.0079788453876972198486328125;
    gauss.y = 0.999800026416778564453125;
    gauss.z = gauss.y * gauss.y;
    float3 avg = float3(0.0);
    float coefficient_sum = 0.0;
    float2 param = p / _31.resolution;
    avg += (get_pixel(param, old, oldSmplr) * gauss.x);
    coefficient_sum += gauss.x;
    float3 _85 = gauss;
    float3 _87 = gauss;
    float2 _89 = _87.xy * _85.yz;
    gauss.x = _89.x;
    gauss.y = _89.y;
    float2 tex_offset = float2(0.25, 0.75);
    for (float i = 1.0; i <= blur_half; i += 1.0)
    {
        float2 param_1 = (p - (tex_offset * i)) / _31.resolution;
        avg += (get_pixel(param_1, old, oldSmplr) * gauss.x);
        float2 param_2 = (p + (tex_offset * i)) / _31.resolution;
        avg += (get_pixel(param_2, old, oldSmplr) * gauss.x);
        coefficient_sum += (2.0 * gauss.x);
        float3 _144 = gauss;
        float3 _146 = gauss;
        float2 _148 = _146.xy * _144.yz;
        gauss.x = _148.x;
        gauss.y = _148.y;
    }
    tex_offset = float2(0.75, 0.25);
    for (float i_1 = 1.0; i_1 <= blur_half; i_1 += 1.0)
    {
        float2 param_3 = (p - (tex_offset * i_1)) / _31.resolution;
        avg += (get_pixel(param_3, old, oldSmplr) * gauss.x);
        float2 param_4 = (p + (tex_offset * i_1)) / _31.resolution;
        avg += (get_pixel(param_4, old, oldSmplr) * gauss.x);
        coefficient_sum += (2.0 * gauss.x);
        float3 _200 = gauss;
        float3 _202 = gauss;
        float2 _204 = _202.xy * _200.yz;
        gauss.x = _204.x;
        gauss.y = _204.y;
    }
    float3 old_px = avg / float3(coefficient_sum);
    float3 new_px = tex.sample(texSmplr, in.tex_coord).xyz;
    out.frag_color = float4(mix(old_px, new_px, float3(powr(_31.progress, 3.0))), 1.0);
    return out;
}

