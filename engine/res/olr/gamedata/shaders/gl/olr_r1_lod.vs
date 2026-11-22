#include "common.h"
out gl_PerVertex { vec4 gl_Position; };
layout(location=POSITION0) in float3 pos0;
layout(location=POSITION1) in float3 pos1;
layout(location=NORMAL0) in float3 normal0;
layout(location=NORMAL1) in float3 normal1;
layout(location=TEXCOORD0) in float2 tc0;
layout(location=TEXCOORD1) in float2 tc1;
layout(location=TEXCOORD2) in float4 rgbh0;
layout(location=TEXCOORD3) in float4 rgbh1;
layout(location=COLOR0) in float4 sun_alpha;
layout(location=TEXCOORD0) out float2 uv0;
layout(location=TEXCOORD1) out float2 uv1;
layout(location=COLOR0) out float4 lighting;
layout(location=COLOR1) out float factor;
layout(location=FOG) out float visibility;
void main()
{
    float4 sun = unpack_D3DCOLOR(sun_alpha);
    factor = sun.w;
    float4 world = float4(mix(pos0, pos1, factor), 1.0);
    float3 normal = normalize(mix(normal0, normal1, factor));
    normal.y += 1.0;
    normal = normalize(normal);
    float4 rgbh = mix(unpack_D3DCOLOR(rgbh0), unpack_D3DCOLOR(rgbh1), factor) * 1.55;
    float sunlight = mix(sun.x, sun.y, factor) * 1.55;
    float3 color = rgbh.rgb + L_hemi_color.rgb * rgbh.w + L_ambient.rgb +
        L_sun_color.rgb * sunlight * (1.0 + 0.7 * dot(normal, L_sun_dir_w));
    lighting = float4(color, sun.z);
    visibility = calc_fogging(world);
    uv0 = tc0;
    uv1 = tc1;
    gl_Position = mul(m_VP, world);
}
