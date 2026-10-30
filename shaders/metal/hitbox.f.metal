// Generated from shaders/hitbox.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 16
// @u bot_left 0 vec2 1 0
// @u top_right 8 vec2 1 0
// @tex tex 0 0
// @in tex_coord 0
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float2 bot_left;
    float2 top_right;
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
float point_in_rect(thread const float2& p, thread const float2& bot_left, thread const float2& top_right)
{
    float2 s = step(bot_left, p) - step(top_right, p);
    return s.x * s.y;
}

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _49 [[buffer(0)]], texture2d<float> tex [[texture(0)]], sampler texSmplr [[sampler(0)]])
{
    main0_out out = {};
    float2 size = float2(int2(tex.get_width(), tex.get_height()));
    float2 tex_bot_left = _49.bot_left / size;
    float2 tex_top_right = _49.top_right / size;
    float4 texel = tex.sample(texSmplr, in.tex_coord);
    float2 param = in.tex_coord;
    float2 param_1 = tex_bot_left;
    float2 param_2 = tex_top_right;
    float t = point_in_rect(param, param_1, param_2);
    out.frag_color = (texel * t) + (float4(0.0) * (1.0 - t));
    return out;
}

