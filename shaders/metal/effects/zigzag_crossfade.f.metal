// Generated from shaders/effects/zigzag_crossfade.f.glsl by shaders/metal/glsl2msl.py; do not edit.
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
float triangle(thread const float& f)
{
    return 1.0 - abs((f * 2.0) - 1.0);
}

static inline __attribute__((always_inline))
float3 get_pixel(thread const float2& xy, texture2d<float> old, sampler oldSmplr, texture2d<float> tex, sampler texSmplr, constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _35)
{
    return mix(old.sample(oldSmplr, xy).xyz, tex.sample(texSmplr, xy).xyz, float3(_35.progress));
}

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _35 [[buffer(0)]], texture2d<float> old [[texture(0)]], texture2d<float> tex [[texture(1)]], sampler oldSmplr [[sampler(0)]], sampler texSmplr [[sampler(1)]])
{
    main0_out out = {};
    float2 p = in.tex_coord * _35.resolution;
    float param = _35.progress;
    float scale = triangle(param) * 100.0;
    float wobble = _35.progress * 24.0;
    float offset = sin((p.x / 10.0) - wobble) * scale;
    float2 param_1 = float2(p.x, p.y + offset) / _35.resolution;
    out.frag_color = float4(get_pixel(param_1, old, oldSmplr, tex, texSmplr, _35), 1.0);
    return out;
}

