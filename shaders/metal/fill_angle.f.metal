// Generated from shaders/fill_angle.f.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 32
// @u center 0 vec2 1 0
// @u start_angle 8 float 1 0
// @u angle 12 float 1 0
// @u color 16 vec4 1 0
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
    float2 center;
    float start_angle;
    float angle;
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

fragment main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _16 [[buffer(0)]])
{
    main0_out out = {};
    float2 p = in.tex_coord - _16.center;
    float a = precise::atan2(p.y, p.x);
    a = mod(a - _16.start_angle, 6.283185482025146484375);
    if (a > _16.angle)
    {
        discard_fragment();
    }
    out.frag_color = _16.color;
    return out;
}

