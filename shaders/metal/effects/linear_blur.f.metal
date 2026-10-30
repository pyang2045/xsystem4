// Generated from shaders/effects/linear_blur.f.glsl by shaders/metal/glsl2msl.py; do not edit.
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
float3 get_pixel(thread const float2& xy, thread const float& fade, texture2d<float> old, sampler oldSmplr, texture2d<float> tex, sampler texSmplr)
{
    return mix(old.sample(oldSmplr, xy).xyz, tex.sample(texSmplr, xy).xyz, float3(fade));
}

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _41 [[buffer(0)]], texture2d<float> old [[texture(0)]], texture2d<float> tex [[texture(1)]], sampler oldSmplr [[sampler(0)]], sampler texSmplr [[sampler(1)]])
{
    main0_out out = {};
    float2 p = in.tex_coord * _41.resolution;
    float fade_progress = 1.0 - powr(_41.progress - 1.0, 2.0);
    float blur_size = powr(sin(3.1415927410125732421875 * _41.progress), 4.0);
    float blur_half = (blur_size * _41.resolution.x) / 2.0;
    float3 avg = float3(0.0);
    float count = 0.0;
    float2 param = p / _41.resolution;
    float param_1 = fade_progress;
    avg += get_pixel(param, param_1, old, oldSmplr, tex, texSmplr);
    count += 1.0;
    for (float i = 1.0; i <= blur_half; i += 2.0)
    {
        float2 param_2 = float2(p.x + i, p.y) / _41.resolution;
        float param_3 = fade_progress;
        avg += get_pixel(param_2, param_3, old, oldSmplr, tex, texSmplr);
        float2 param_4 = float2(p.x - i, p.y) / _41.resolution;
        float param_5 = fade_progress;
        avg += get_pixel(param_4, param_5, old, oldSmplr, tex, texSmplr);
        count += 2.0;
    }
    out.frag_color = float4(avg / float3(count), 1.0);
    return out;
}

