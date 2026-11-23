#include "common.h"
out gl_PerVertex { vec4 gl_Position; };
layout(location = POSITION) in float4 vertex_position;
layout(location = COLOR0) in float4 vertex_color;
layout(location = TEXCOORD0) in float2 vertex_uv;
layout(location = COLOR0) out float4 shadow_color;
layout(location = TEXCOORD0) out float2 shadow_uv;
void main()
{
    gl_Position=mul(m_WVP,vertex_position);
    shadow_color=unpack_D3DCOLOR(vertex_color);
    shadow_uv=vertex_uv;
}
