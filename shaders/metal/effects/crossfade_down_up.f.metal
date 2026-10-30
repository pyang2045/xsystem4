// Generated from shaders/effects/crossfade_down_up.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 12
// @u resolution 0 vec2 1 0
// @u progress 8 float 1 0
// @tex tex 0 0
// @tex old 1 1
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

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _12 [[buffer(0)]], texture2d<float> tex [[texture(0)]], texture2d<float> old [[texture(1)]], sampler texSmplr [[sampler(0)]], sampler oldSmplr [[sampler(1)]])
{
    main0_out out = {};
    float top = ((_12.resolution.y + 210.0) * _12.progress) - 210.0;
    float p = (1.0 - in.tex_coord.y) * _12.resolution.y;
    float alpha = fast::clamp((p - top) / 210.0, 0.0, 1.0);
    float3 col = mix(tex.sample(texSmplr, in.tex_coord).xyz, old.sample(oldSmplr, in.tex_coord).xyz, float3(alpha));
    out.frag_color = float4(col, 1.0);
    return out;
}

