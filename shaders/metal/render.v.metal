// Generated from shaders/render.v.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 128
// @u world_transform 0 mat4 1 0
// @u view_transform 64 mat4 1 0
// @attr vertex_pos 0
// @attr vertex_uv 1
// @out tex_coord 0
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float4x4 world_transform;
    float4x4 view_transform;
};

struct main0_out
{
    float2 tex_coord [[user(locn0)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float4 vertex_pos [[attribute(0)]];
    float2 vertex_uv [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _19 [[buffer(0)]], constant float4& xs4_flip [[buffer(30)]])
{
    main0_out out = {};
    out.gl_Position = (_19.view_transform * _19.world_transform) * in.vertex_pos;
    out.tex_coord = in.vertex_uv;
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y *= xs4_flip.x;
    return out;
}

