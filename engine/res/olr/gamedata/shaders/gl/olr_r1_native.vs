#include "common.h"

out gl_PerVertex { vec4 gl_Position; };
layout(location = POSITION) in float4 vertex_position;
layout(location = NORMAL) in float4 vertex_normal;
layout(location = TANGENT) in float4 vertex_tangent;
layout(location = BINORMAL) in float4 vertex_binormal;
layout(location = TEXCOORD0) in float2 vertex_uv;
layout(location = TEXCOORD0) out float2 base_uv;
layout(location = FOG) out float fog_visibility;

#ifdef OLR_R1_LOCAL
uniform float4 olr_r1_dynamic_pos;
uniform float4 olr_r1_dynamic_color;
uniform float4x4 olr_r1_light_xform;
layout(location = COLOR0) out float3 local_color;
layout(location = TEXCOORD1) out float4 attenuation_uv;
layout(location = TEXCOORD2) out float2 clip_uv;
#else
layout(location = COLOR0) out float3 hemi_light;
layout(location = COLOR1) out float3 sun_light;
#ifdef OLR_R1_VERTEX
layout(location = COLOR0) in float4 vertex_color;
layout(location = TEXCOORD1) out float3 baked_vertex;
#else
layout(location = TEXCOORD1) in float2 vertex_lightmap_uv;
layout(location = TEXCOORD1) out float2 lightmap_uv;
#endif
#ifdef OLR_R1_DETAIL
layout(location = TEXCOORD3) out float2 detail_uv;
layout(location = TEXCOORD4) out float2 detail_weights;
#endif
#endif

void main()
{
    float4 normal_packed = unpack_D3DCOLOR(vertex_normal);
    float3 normal = unpack_normal(normal_packed);
    base_uv = unpack_tc_base(vertex_uv, unpack_D3DCOLOR(vertex_tangent).w,
        unpack_D3DCOLOR(vertex_binormal).w);
#if defined(OLR_R1_SCENE) && !defined(OLR_R1_LOCAL)
    // Preserve the vertex fog output until rasterization, as r1/lmap.vs.
    fog_visibility = calc_fogging(vertex_position);
#else
    fog_visibility = saturate(calc_fogging(vertex_position));
#endif
    gl_Position = mul(m_WVP, vertex_position);
#ifdef OLR_R1_LOCAL
    // OLR r1/common.h calc_point/calc_spot: calculate Gouraud light at
    // vertices, then interpolate COLOR. Do not replace with per-pixel Lambert.
    float3 delta = vertex_position.xyz - olr_r1_dynamic_pos.xyz;
    float3 ray = delta * inversesqrt(max(dot(delta, delta), 1e-12));
    local_color = olr_r1_dynamic_color.rgb * dot(normal, -ray) * fog_visibility;
#ifdef OLR_R1_SPOT
    float4 projected = mul(olr_r1_light_xform, vertex_position);
    attenuation_uv = projected.xyww;
    clip_uv = float2(projected.z);
#else
    float3 tc = delta * olr_r1_dynamic_pos.w + 0.5;
    attenuation_uv = float4(tc.xz, 0.0, 1.0);
    clip_uv = tc.xy;
#endif
#else
    hemi_light = L_hemi_color.rgb * (0.98 + 0.2 * normal.y);
    sun_light = L_sun_color.rgb * max(0.3, dot(normal, -L_sun_dir_w.xyz));
#ifdef OLR_R1_VERTEX
    float4 color = unpack_D3DCOLOR(vertex_color);
    baked_vertex = color.rgb;
    hemi_light *= normal_packed.w;
    sun_light *= color.a;
#else
    lightmap_uv = unpack_tc_lmap(vertex_lightmap_uv);
#endif
#ifdef OLR_R1_DETAIL
    float d = distance(vertex_position.xyz, eye_position) * dt_params.w;
    float fade = min(d * d, 1.0);
    detail_uv = base_uv * dt_params.xy;
    detail_weights = float2(1.0 - fade, 0.5 * fade);
#endif
#endif
}
