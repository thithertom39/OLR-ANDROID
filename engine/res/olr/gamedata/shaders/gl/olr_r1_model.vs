#include "common.h"
#include "skin.h"

out gl_PerVertex { vec4 gl_Position; };
layout(location = POSITION) in float4 vertex_position;
layout(location = NORMAL) in float4 vertex_normal;
layout(location = TANGENT) in float4 vertex_tangent;
layout(location = BINORMAL) in float4 vertex_binormal;
layout(location = TEXCOORD0) in float4 vertex_uv;
#ifdef SKIN_4
layout(location = TEXCOORD1) in float4 vertex_indices;
#endif
layout(location = TEXCOORD0) out float2 base_uv;
layout(location = FOG) out float fog_visibility;
#ifdef OLR_R1_LOCAL
uniform float4 olr_r1_dynamic_pos, olr_r1_dynamic_color;
uniform float4x4 olr_r1_light_xform;
layout(location = COLOR0) out float3 local_color;
layout(location = TEXCOORD1) out float4 attenuation_uv;
layout(location = TEXCOORD2) out float2 clip_uv;
#else
uniform float4 olr_r1_model_props;
layout(location = COLOR0) out float3 sun_color;
layout(location = COLOR1) out float4 model_color;
#ifdef OLR_R1_MODEL_HQ
uniform float4x4 olr_r1_model_projector;
uniform float4 olr_r1_model_clamp[2];
layout(location = TEXCOORD1) out float4 projector_uv;
#endif
#endif

v_model vertex_model()
{
#if defined(SKIN_0)
    v_model_skinned_0 v;
    v.P=vertex_position; v.N=vertex_normal.xyz; v.T=vertex_tangent.xyz; v.B=vertex_binormal.xyz; v.tc=vertex_uv.xy;
    return skinning_0(v);
#elif defined(SKIN_1)
    v_model_skinned_1 v;
    v.P=vertex_position; v.N=vertex_normal; v.T=vertex_tangent.xyz; v.B=vertex_binormal.xyz; v.tc=vertex_uv.xy;
    return skinning_1(v);
#elif defined(SKIN_2)
    v_model_skinned_2 v;
    v.P=vertex_position; v.N=vertex_normal; v.T=vertex_tangent.xyz; v.B=vertex_binormal.xyz; v.tc=vertex_uv;
#if !defined(OLR_R1_MODEL_HQ) && !defined(OLR_R1_LOCAL)
    // OLR model_def_lq uses skinning_2lq: the first bone only. Keep the
    // GLES loader's converted positions/UVs and D3DCOLOR swizzle.
    int bone = int(v.tc.z);
    v_model result;
    result.P = skinning_pos(v.P,sbones_array[bone],sbones_array[bone+1],sbones_array[bone+2]);
    result.N = skinning_dir(v.N.zyx,sbones_array[bone],sbones_array[bone+1],sbones_array[bone+2]);
    result.T = skinning_dir(v.T.zyx,sbones_array[bone],sbones_array[bone+1],sbones_array[bone+2]);
    result.B = skinning_dir(v.B.zyx,sbones_array[bone],sbones_array[bone+1],sbones_array[bone+2]);
    result.tc = v.tc.xy;
    return result;
#else
    return skinning_2(v);
#endif
#elif defined(SKIN_3)
    v_model_skinned_3 v;
    v.P=vertex_position; v.N=vertex_normal; v.T=vertex_tangent; v.B=vertex_binormal; v.tc=vertex_uv;
    return skinning_3(v);
#elif defined(SKIN_4)
    v_model_skinned_4 v;
    v.P=vertex_position; v.N=vertex_normal; v.T=vertex_tangent; v.B=vertex_binormal; v.tc=vertex_uv.xy; v.ind=vertex_indices;
    return skinning_4(v);
#else
    v_model v;
    v.P=vertex_position; v.N=vertex_normal.xyz; v.T=vertex_tangent.xyz; v.B=vertex_binormal.xyz; v.tc=vertex_uv.xy;
    return v;
#endif
}

void main()
{
    v_model vertex = vertex_model();
    float3 position = mul(m_W,vertex.P);
    float3 normal = normalize(mul(m_W,vertex.N));
    base_uv = vertex.tc;
    gl_Position = mul(m_WVP,vertex.P);
    fog_visibility = calc_fogging(float4(position,1.0));
#ifdef OLR_R1_LOCAL
    fog_visibility = saturate(fog_visibility);
    float3 delta = position-olr_r1_dynamic_pos.xyz;
    float3 ray = delta*inversesqrt(max(dot(delta,delta),1e-12));
    local_color = olr_r1_dynamic_color.rgb*dot(normal,-ray)*fog_visibility;
#ifdef OLR_R1_SPOT
    float4 projected = mul(olr_r1_light_xform,float4(position,1.0));
    attenuation_uv = projected.xyww;
    clip_uv = float2(projected.z);
#else
    float3 tc = delta*olr_r1_dynamic_pos.w+.5;
    attenuation_uv = float4(tc.xz,0.0,1.0);
    clip_uv = tc.xy;
#endif
#else
    sun_color = L_sun_color.rgb*max(dot(normal,-L_sun_dir_w),0.0);
    float3 hemi = (normal.y*.5+.5)*olr_r1_model_props.w*L_hemi_color.rgb;
    model_color = float4(hemi+L_ambient.rgb+olr_r1_model_props.xyz*sun_color,1.0);
#ifdef OLR_R1_MODEL_HQ
    // Explicit min/max preserves the HLSL order even for tiny object bounds
    // whose original 0.05 shrink would invert a clamp interval.
    float3 clamped = min(max(position,olr_r1_model_clamp[0].xyz),olr_r1_model_clamp[1].xyz);
    float4 projected = mul(olr_r1_model_projector,float4(clamped,1.0));
    projector_uv = projected.xyww;
    model_color.w = olr_r1_model_clamp[0].w;
#endif
#endif
}
