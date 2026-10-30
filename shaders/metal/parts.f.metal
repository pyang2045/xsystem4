// Generated from shaders/parts.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 80
// @u blend_rate 0 float 1 0
// @u bot_left 8 vec2 1 0
// @u top_right 16 vec2 1 0
// @u add_color 32 vec3 1 0
// @u multiply_color 48 vec3 1 0
// @u draw_filter 60 int 1 0
// @u add_after_multiply 64 int 1 0
// @u use_clipper 68 int 1 0
// @u use_clip_area 72 int 1 0
// @u use_persp 76 int 1 0
// @tex tex 0 0
// @tex clipper_tex 1 1
// @in clip_coord 1
// @in clip_area_coord 2
// @in tex_coord 0
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float blend_rate;
    float2 bot_left;
    float2 top_right;
    float3 add_color;
    packed_float3 multiply_color;
    int draw_filter;
    int add_after_multiply;
    int use_clipper;
    int use_clip_area;
    int use_persp;
};

struct main0_out
{
    float4 frag_color [[color(0)]];
};

struct main0_in
{
    float2 tex_coord [[user(locn0)]];
    float3 clip_coord [[user(locn1)]];
    float3 clip_area_coord [[user(locn2)]];
};

static inline __attribute__((always_inline))
bool inside_rect(thread const float2& p, thread const float2& bl, thread const float2& tr)
{
    bool _20 = all(p >= bl);
    bool _27;
    if (_20)
    {
        _27 = all(p < tr);
    }
    else
    {
        _27 = _20;
    }
    return _27;
}

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _35 [[buffer(0)]], texture2d<float> tex [[texture(0)]], texture2d<float> clipper_tex [[texture(1)]], sampler texSmplr [[sampler(0)]], sampler clipper_texSmplr [[sampler(1)]])
{
    main0_out out = {};
    float2 _42;
    if (_35.use_persp != 0)
    {
        _42 = in.clip_coord.xy / float2(in.clip_coord.z);
    }
    else
    {
        _42 = in.clip_coord.xy;
    }
    float2 clip_xy = _42;
    float2 _64;
    if (_35.use_persp != 0)
    {
        _64 = in.clip_area_coord.xy / float2(in.clip_area_coord.z);
    }
    else
    {
        _64 = in.clip_area_coord.xy;
    }
    float2 clip_area_xy = _64;
    bool _81 = _35.use_clip_area != 0;
    bool _94;
    if (_81)
    {
        float2 param = clip_area_xy;
        float2 param_1 = float2(0.0);
        float2 param_2 = float2(1.0);
        _94 = !inside_rect(param, param_1, param_2);
    }
    else
    {
        _94 = _81;
    }
    if (_94)
    {
        discard_fragment();
    }
    float2 size = float2(int2(tex.get_width(), tex.get_height()));
    float2 param_3 = in.tex_coord;
    float2 param_4 = _35.bot_left / size;
    float2 param_5 = _35.top_right / size;
    if (!inside_rect(param_3, param_4, param_5))
    {
        discard_fragment();
    }
    float4 tex_color = tex.sample(texSmplr, in.tex_coord);
    float3 mod_color;
    if (_35.add_after_multiply != 0)
    {
        mod_color = fast::clamp((tex_color.xyz * float3(_35.multiply_color)) + _35.add_color, float3(0.0), float3(1.0));
    }
    else
    {
        mod_color = (tex_color.xyz + _35.add_color) * float3(_35.multiply_color);
    }
    float alpha = tex_color.w * _35.blend_rate;
    if (_35.use_clipper != 0)
    {
        float2 param_6 = clip_xy;
        float2 param_7 = float2(0.0);
        float2 param_8 = float2(1.0);
        if (!inside_rect(param_6, param_7, param_8))
        {
            discard_fragment();
        }
        alpha *= clipper_tex.sample(clipper_texSmplr, clip_xy).w;
    }
    if (_35.draw_filter == 2)
    {
        out.frag_color = float4(mix(float3(1.0), mod_color, float3(alpha)), 1.0);
    }
    else
    {
        if (_35.draw_filter == 3)
        {
            out.frag_color = float4(mod_color * alpha, 1.0);
        }
        else
        {
            out.frag_color = float4(mod_color, alpha);
        }
    }
    return out;
}

