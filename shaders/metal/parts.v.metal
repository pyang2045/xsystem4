// Generated from shaders/parts.v.glsl by shaders/metal/glsl2msl.py; do not edit.
// @ubo 0 336
// @u world_transform 0 mat4 1 0
// @u view_transform 64 mat4 1 0
// @u persp_transform 128 mat4 1 0
// @u use_persp 192 int 1 0
// @u inv_clipper_transform 208 mat4 1 0
// @u inv_clip_area_transform 272 mat4 1 0
// @attr vertex_pos 0
// @attr vertex_uv 1
// @out tex_coord 0
// @out clip_coord 1
// @out clip_area_coord 2
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock
{
    float4x4 world_transform;
    float4x4 view_transform;
    float4x4 persp_transform;
    int use_persp;
    float4x4 inv_clipper_transform;
    float4x4 inv_clip_area_transform;
};

struct main0_out
{
    float2 tex_coord [[user(locn0)]];
    float3 clip_coord [[user(locn1)]];
    float3 clip_area_coord [[user(locn2)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float4 vertex_pos [[attribute(0)]];
    float2 vertex_uv [[attribute(1)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant _RESERVED_IDENTIFIER_FIXUP_gl_DefaultUniformBlock& _14 [[buffer(0)]], constant float4& xs4_flip [[buffer(30)]])
{
    main0_out out = {};
    float4 world_pos = _14.world_transform * in.vertex_pos;
    out.tex_coord = in.vertex_uv;
    if (_14.use_persp != 0)
    {
        float4 screen_pos = _14.persp_transform * world_pos;
        out.gl_Position = _14.view_transform * screen_pos;
        float4 flat_pos = float4(screen_pos.xy / float2(screen_pos.w), 0.0, 1.0);
        out.clip_coord = float3((_14.inv_clipper_transform * flat_pos).xy * screen_pos.w, screen_pos.w);
        out.clip_area_coord = float3((_14.inv_clip_area_transform * flat_pos).xy * screen_pos.w, screen_pos.w);
    }
    else
    {
        out.gl_Position = _14.view_transform * world_pos;
        out.clip_coord = float3((_14.inv_clipper_transform * world_pos).xy, 1.0);
        out.clip_area_coord = float3((_14.inv_clip_area_transform * world_pos).xy, 1.0);
    }
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y *= xs4_flip.x;
    return out;
}

