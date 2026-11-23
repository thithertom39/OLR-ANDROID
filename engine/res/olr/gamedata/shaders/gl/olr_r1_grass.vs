#include "common.h"
out gl_PerVertex { vec4 gl_Position; };
layout(location=POSITION) in float4 vertex_position;
layout(location=TEXCOORD0) in float4 misc;
layout(location=TEXCOORD0) out float2 uv;
layout(location=COLOR0) out float3 color;
layout(location=FOG) out float visibility;
uniform float4 consts, wave, dir2D;
uniform float4 array[61*4];
void main()
{
    int index = int(misc.w);
    float4 m0 = array[index], m1 = array[index+1], m2 = array[index+2];
    float4 world = float4(dot(m0,vertex_position), dot(m1,vertex_position), dot(m2,vertex_position), 1.0);
    color = array[index+3].rgb;
#ifndef OLR_R1_GRASS_STILL
    float cycle = calc_cyclic(dot(world,wave));
    float rigidity = misc.z * consts.x;
    float2 displacement = calc_xz_wave(dir2D.xz * ((world.y-m1.w)*cycle),rigidity);
    world.xz += displacement;
    color *= consts.w + consts.z * max(0.0,cycle) * rigidity;
#endif
    uv = misc.xy * consts.xy;
    visibility = calc_fogging(world);
    gl_Position = mul(m_WVP,world);
}
