// Generated from shaders/amap_saturate.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @tex tex 0 0
// @in tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 frag_color [[color(0)]];
};

struct main0_in
{
    float2 tex_coord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler texSmplr [[sampler(0)]])
{
    main0_out out = {};
    float alpha = tex.sample(texSmplr, in.tex_coord).w;
    float on = step(0.001000000047497451305389404296875, alpha);
    out.frag_color = float4(on, on, on, alpha);
    return out;
}

