// Generated from shaders/movie.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @tex texture_y 0 0
// @tex texture_cb 1 1
// @tex texture_cr 2 2
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

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> texture_y [[texture(0)]], texture2d<float> texture_cb [[texture(1)]], texture2d<float> texture_cr [[texture(2)]], sampler texture_ySmplr [[sampler(0)]], sampler texture_cbSmplr [[sampler(1)]], sampler texture_crSmplr [[sampler(2)]])
{
    main0_out out = {};
    float4x4 rec601 = float4x4(float4(1.16437995433807373046875, 0.0, 1.5960299968719482421875, -0.870790004730224609375), float4(1.16437995433807373046875, -0.39175999164581298828125, -0.812969982624053955078125, 0.52959001064300537109375), float4(1.16437995433807373046875, 2.01723003387451171875, 0.0, -1.08139002323150634765625), float4(0.0, 0.0, 0.0, 1.0));
    float y = texture_y.sample(texture_ySmplr, in.tex_coord).x;
    float cb = texture_cb.sample(texture_cbSmplr, in.tex_coord).x;
    float cr = texture_cr.sample(texture_crSmplr, in.tex_coord).x;
    out.frag_color = float4(y, cb, cr, 1.0) * rec601;
    return out;
}

