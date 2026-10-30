// Generated from shaders/dilate.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 32
// @u threshold 0 float 1 0
// @u color 16 vec4 1 0
// @tex tex 0 0
// @in tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float threshold;
    float4 color;
};

struct main0_out
{
    float4 frag_color [[color(0)]];
};

struct main0_in
{
    float2 tex_coord [[user(locn0)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _13 [[buffer(0)]], texture2d<float> tex [[texture(0)]], sampler texSmplr [[sampler(0)]])
{
    main0_out out = {};
    int size = int(floor(_13.threshold));
    int cutoff = size + 1;
    float edge_weight = fract(_13.threshold) / 4.0;
    float2 tex_size = float2(int2(tex.get_width(), tex.get_height()));
    float t = ceil(_13.threshold);
    float a_out = 0.0;
    int _51 = -cutoff;
    for (int x = _51; x <= cutoff; x++)
    {
        int _63 = -cutoff;
        for (int y = _63; y <= cutoff; y++)
        {
            float d = distance(float2(float(x), float(y)), float2(0.0));
            if (d > t)
            {
                continue;
            }
            float a = tex.sample(texSmplr, (in.tex_coord + (float2(float(x), float(y)) / tex_size))).x;
            if (d > float(size))
            {
                a *= edge_weight;
            }
            a_out += a;
        }
    }
    if (a_out < _13.color.w)
    {
        discard_fragment();
    }
    out.frag_color = float4(_13.color.xyz, fast::min(a_out, 1.0));
    return out;
}

