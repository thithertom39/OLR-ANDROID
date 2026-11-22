#include "common.h"

out gl_PerVertex { vec4 gl_Position; };
layout(location = POSITION) in float4 vertex_position;
layout(location = NORMAL) in float4 vertex_normal;
layout(location = TEXCOORD0) out float2 surface_uv;
layout(location = COLOR0) out float4 surface_color;
uniform float4 olr_r1_projector_raster;
#ifdef OLR_R1_PROJECTOR_VERTEX
layout(location = COLOR0) in float4 vertex_color;
#elif defined(OLR_R1_PROJECTOR_IMPLICIT)
layout(location = TANGENT) in float4 vertex_tangent;
layout(location = BINORMAL) in float4 vertex_binormal;
layout(location = TEXCOORD0) in float2 vertex_uv;
#else
layout(location = TEXCOORD1) in float2 vertex_lightmap_uv;
#endif

void main()
{
    float4 normal = unpack_D3DCOLOR(vertex_normal);
    float3 hemi = L_hemi_color.rgb * (0.98 + 0.2 * unpack_normal(normal).y);
    surface_uv = float2(0.0);
    surface_color = float4(hemi, 1.0);
#ifdef OLR_R1_PROJECTOR_VERTEX
    float4 baked = unpack_D3DCOLOR(vertex_color);
    surface_color = float4(baked.rgb + hemi * normal.w + L_ambient.rgb, baked.a);
#elif defined(OLR_R1_PROJECTOR_IMPLICIT)
    surface_uv = unpack_tc_base(vertex_uv, unpack_D3DCOLOR(vertex_tangent).w, unpack_D3DCOLOR(vertex_binormal).w);
#else
    surface_uv = unpack_tc_lmap(vertex_lightmap_uv);
#endif
    float4 clip = mul(m_VP, vertex_position);
    // This atlas emulates D3D9 raster coordinates for the original UVgen:
    // logical tile rows map to GLES bottom-up storage; integer pixel centers
    // map to GL half-integer centers. This is atlas-only, not world geometry.
    gl_Position = float4(clip.x, -clip.y, 2.0 * clip.z - clip.w, clip.w);
    gl_Position.xy += olr_r1_projector_raster.xy * clip.w;
}
