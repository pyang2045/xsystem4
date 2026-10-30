// Generated from shaders/effects/mosaic.f.glsl by shaders/metal/glsl2msl.py; do not edit.
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

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

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

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _30 [[buffer(0)]], texture2d<float> old [[texture(0)]], texture2d<float> tex [[texture(1)]], sampler oldSmplr [[sampler(0)]], sampler texSmplr [[sampler(1)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    float stride = 128.0 * (1.0 - abs((_30.progress * 2.0) - 1.0));
    float2 offset = mod(gl_FragCoord.xy, float2(stride));
    float2 chunk = in.tex_coord - (offset / _30.resolution);
    float2 center = chunk + (float2(stride / 2.0, stride / 2.0) / _30.resolution);
    float2 param = center;
    out.frag_color = float4(get_pixel(param, old, oldSmplr, tex, texSmplr, _30), 1.0);
    return out;
}

