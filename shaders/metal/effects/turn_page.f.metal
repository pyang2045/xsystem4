// Generated from shaders/effects/turn_page.f.glsl by shaders/metal/glsl2msl.py; do not edit.
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
    float2 p = in.tex_coord * _15.resolution;
    float2 p1 = _15.resolution;
    float2 p2 = float2((1.0 - (fast::max(_15.progress, 0.00999999977648258209228515625) * 2.0)) * p1.x, (1.0 - (fast::max(_15.progress, 0.00999999977648258209228515625) * 2.0)) * p1.y);
    float2 p3 = float2(p2.x, _15.resolution.y);
    float2 p4 = float2(_15.resolution.x, p2.y);
    float b_below_line = step(0.0, ((p4.x - p3.x) * (p.y - p3.y)) - ((p4.y - p3.y) * (p.x - p3.x)));
    float b_old = step(0.5, step(p.x, p2.x) + step(p.y, p2.y));
    float b_mirror = abs(1.0 - b_old) * abs(1.0 - b_below_line);
    float b_new = b_below_line;
    float2 p_diff = p - p2;
    float3 c_old = old.sample(oldSmplr, in.tex_coord).xyz;
    float3 c_mirror = old.sample(oldSmplr, (float2(p1.x - p_diff.x, p1.y - p_diff.y) / _15.resolution)).xyz;
    float3 c_new = tex.sample(texSmplr, in.tex_coord).xyz;
    float3 col = ((c_old * b_old) + (c_mirror * b_mirror)) + (c_new * b_new);
    out.frag_color = float4(col, 1.0);
    return out;
}

