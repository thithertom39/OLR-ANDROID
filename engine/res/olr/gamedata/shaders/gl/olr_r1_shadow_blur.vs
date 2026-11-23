#include "common.h"
out gl_PerVertex { vec4 gl_Position; };
layout(location = POSITION) in float4 vertex_position;
layout(location = TEXCOORD0) in float2 vertex_uv;
layout(location = TEXCOORD0) out float2 shadow_uv;
void main() { gl_Position=vertex_position; shadow_uv=vertex_uv; }
