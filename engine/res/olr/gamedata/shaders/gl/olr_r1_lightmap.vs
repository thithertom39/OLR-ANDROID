#include "common.h"

out gl_PerVertex { vec4 gl_Position; };

layout(location = POSITION) in float4 vertex_position;
layout(location = NORMAL) in float4 vertex_normal;
layout(location = TANGENT) in float4 vertex_tangent;
layout(location = BINORMAL) in float4 vertex_binormal;
layout(location = TEXCOORD0) in float2 vertex_uv;
layout(location = TEXCOORD0) out float2 base_uv;
layout(location = FOG) out float fog_visibility;
#ifdef OLR_R1_DETAIL
layout(location = TEXCOORD3) out float2 detail_uv;
layout(location = TEXCOORD4) out float2 detail_weights;
#endif
#ifdef OLR_R1_VERTEX
layout(location = COLOR0) in float4 vertex_color;
layout(location = COLOR0) out float3 vertex_light;
#else
layout(location = TEXCOORD1) in float2 vertex_lightmap_uv;
layout(location = TEXCOORD1) out float2 lightmap_uv;
layout(location = COLOR0) out float3 hemi_light;
layout(location = COLOR1) out float3 sun_light;
#endif

void main()
{
    float4 packed_normal = unpack_D3DCOLOR(vertex_normal);
    float3 normal = unpack_normal(packed_normal);
    float4 tangent = unpack_D3DCOLOR(vertex_tangent);
    float4 binormal = unpack_D3DCOLOR(vertex_binormal);
    base_uv = unpack_tc_base(vertex_uv, tangent.w, binormal.w);
#ifdef OLR_R1_DETAIL
    float detail_distance = distance(vertex_position.xyz, eye_position) * dt_params.w;
    float detail_fade = min(detail_distance * detail_distance, 1.0);
    detail_uv = base_uv * dt_params.xy;
    detail_weights = float2(1.0 - detail_fade, 0.5 * detail_fade);
#endif
    float3 hemi = L_hemi_color.rgb * (0.98 + 0.2 * normal.y);
    float3 sun = L_sun_color.rgb * max(0.3, dot(normal, -L_sun_dir_w.xyz));
#ifdef OLR_R1_VERTEX
    float4 baked = unpack_D3DCOLOR(vertex_color);
    // R2 already adds ambient/hemi/sun; retain only the baked vertex term.
    vertex_light = baked.rgb;
#else
    lightmap_uv = unpack_tc_lmap(vertex_lightmap_uv);
    hemi_light = hemi;
    sun_light = sun;
#endif
    fog_visibility = saturate(calc_fogging(vertex_position));
    gl_Position = mul(m_WVP, vertex_position);
}
