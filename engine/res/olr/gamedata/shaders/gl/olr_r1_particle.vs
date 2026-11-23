#include "common.h"

out gl_PerVertex { vec4 gl_Position; };
layout(location = POSITION) in float4 vertex_position;
layout(location = COLOR0) in float4 vertex_color;
layout(location = TEXCOORD0) in float2 vertex_uv;
layout(location = TEXCOORD0) out float2 particle_uv;
layout(location = COLOR0) out float4 particle_color;
layout(location = FOG) out float fog_visibility;

void main()
{
    // OLR r1/particle.vs: submitted particle positions are world-space.
    gl_Position = mul(m_WVP, vertex_position);
    particle_uv = vertex_uv;
    particle_color = unpack_D3DCOLOR(vertex_color);
    fog_visibility = calc_fogging(vertex_position);
}
