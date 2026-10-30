// Generated from shaders/copy_grayscale.f.glsl by shaders/metal/glsl2msl.py; do not edit.
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
    float4 texel = tex.sample(texSmplr, in.tex_coord);
    float gray = dot(texel.xyz, float3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    out.frag_color = float4(float3(gray), texel.w);
    return out;
}

