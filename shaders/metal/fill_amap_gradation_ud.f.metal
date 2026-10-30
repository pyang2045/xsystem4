// Generated from shaders/fill_amap_gradation_ud.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 8
// @u threshold 0 float 1 0
// @u threshold2 4 float 1 0
// @tex tex 0 0
// @in tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float threshold;
    float threshold2;
};

struct main0_out
{
    float4 frag_color [[color(0)]];
};

struct main0_in
{
    float2 tex_coord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _11 [[buffer(0)]], texture2d<float> tex [[texture(0)]], sampler texSmplr [[sampler(0)]])
{
    main0_out out = {};
    float rate = _11.threshold + ((_11.threshold2 - _11.threshold) * in.tex_coord.y);
    out.frag_color = float4(tex.sample(texSmplr, in.tex_coord).xyz, rate);
    return out;
}

