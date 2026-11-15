#include "stdafx.h"
#pragma hdrstop

#include "Layers/xrRender/blenders/uber_deffer.h"
#include "blender_deffer_flat.h"

namespace xray::render::RENDER_NAMESPACE
{
CBlender_deffer_flat::CBlender_deffer_flat()
{
    description.CLS = B_DEFAULT;
    description.version = 1;
    oTessellation.Count = 4;
    oTessellation.IDselected = 0;
}

CBlender_deffer_flat::~CBlender_deffer_flat() {}
void CBlender_deffer_flat::Save(IWriter& fs)
{
    IBlender::Save(fs);
    xrP_TOKEN::Item I;
    xrPWRITE_PROP(fs, "Tessellation", xrPID_TOKEN, oTessellation);
    I.ID = 0;
    xr_strcpy(I.str, "NO_TESS");
    fs.w(&I, sizeof(I));
    I.ID = 1;
    xr_strcpy(I.str, "TESS_PN");
    fs.w(&I, sizeof(I));
    I.ID = 2;
    xr_strcpy(I.str, "TESS_HM");
    fs.w(&I, sizeof(I));
    I.ID = 3;
    xr_strcpy(I.str, "TESS_PN+HM");
    fs.w(&I, sizeof(I));
}
void CBlender_deffer_flat::Load(IReader& fs, u16 version)
{
    IBlender::Load(fs, version);
    if (version > 0)
    {
        xrPREAD_PROP(fs, xrPID_TOKEN, oTessellation);
        oTessellation.Count = 4;
    }
}

void CBlender_deffer_flat::Compile(CBlender_Compile& C)
{
    IBlender::Compile(C);

#if RENDER == R_R2
    // codepath is the same, only the shaders differ
    switch (C.iElement)
    {
    case SE_R2_NORMAL_HQ: // deffer
        uber_deffer(C, true, "base", "base", false);
        break;
    case SE_R2_NORMAL_LQ: // deffer
        uber_deffer(C, false, "base", "base", false);
        break;
    case SE_R2_SHADOW: // smap-direct
        if (RImplementation.o.HW_smap)
            C.r_Pass("shadow_direct_base", "dumb", FALSE, TRUE, TRUE, FALSE);
        else
            C.r_Pass("shadow_direct_base", "shadow_direct_base", FALSE);
        C.r_Sampler("s_base", C.L_textures[0]);
        C.r_End();
        break;
    }
#elif RENDER == R_GL
    // codepath is the same, only the shaders differ
    switch (C.iElement)
    {
    case SE_R2_NORMAL_HQ: // deffer
        uber_deffer(C, true, "base", "base", false, nullptr, true);

        C.r_Stencil(TRUE, D3DCMP_ALWAYS, 0xff, 0x7f, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
        C.r_StencilRef(0x01);
        C.r_End();
        break;
    case SE_R2_NORMAL_LQ: // deffer
        uber_deffer(C, false, "base", "base", false, nullptr, true);

        C.r_Stencil(TRUE, D3DCMP_ALWAYS, 0xff, 0x7f, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
        C.r_StencilRef(0x01);
        C.r_End();
        break;
    case SE_R2_SHADOW: // smap-direct
        //if (RImplementation.o.HW_smap)	C.r_Pass	("shadow_direct_base","dumb",	FALSE,TRUE,TRUE,FALSE);
        //else							C.r_Pass	("shadow_direct_base","shadow_direct_base",FALSE);
        // Monolithic GL programs need a fragment shader for depth rasterization.
        C.r_Pass("shadow_direct_base", "dumb", FALSE, TRUE,TRUE,FALSE);
        C.r_ColorWriteEnable(false, false, false, false);
        C.r_End();
        break;
#if defined(XR_PLATFORM_ANDROID)
    case SE_OLR_R1_LIGHTMAP:
    {
        const bool vertexLit = description.CLS == B_VERT;
        if (!vertexLit && (C.L_textures.size() != 3 ||
            strncmp(C.L_textures[1].c_str(), "lmap#", 5) || strncmp(C.L_textures[2].c_str(), "lmap#", 5)))
            break;

        // Use the original diffuse-detail metadata. The R2 compiler can reclassify bump-only detail.
        bool diffuseDetail = false;
        bool bumpDetail = false;
        RImplementation.Resources->m_textures_description.GetTextureUsage(C.L_textures[0], diffuseDetail, bumpDetail);
        const bool hasDetail = diffuseDetail && C.detail_texture && C.detail_scaler;
        pcstr shader = vertexLit ? "olr_r1_vertex" : "olr_r1_lightmap";
        if (hasDetail)
            shader = vertexLit ? "olr_r1_vertex_detail" : "olr_r1_lightmap_detail";

        // Add only the baked diffuse contribution. The deferred R2 lighting
        // (flashlight/point lights, hemisphere, ambient) must stay intact.
        // HDR float blending is not available on all GLES devices.
        const bool hdrAccumulator = RImplementation.o.fp16_blend;
        const pcstr pixelShader = hdrAccumulator
            ? (vertexLit ? "olr_r1_vertex_accum" : "olr_r1_lightmap_accum")
            : shader;
        C.r_Pass(shader, pixelShader, FALSE, TRUE, FALSE, TRUE, D3DBLEND_ONE, D3DBLEND_ONE);
        C.RS.SetRS(D3DRS_ZFUNC, D3DCMP_EQUAL);
        if (!hdrAccumulator)
            C.r_Sampler("s_base", C.L_textures[0]);
        if (!vertexLit)
            C.r_Sampler_clf("s_lmap", C.L_textures[1].c_str());
        if (hasDetail && !hdrAccumulator)
            C.r_Sampler("s_detail", C.detail_texture);
        C.r_End();
        break;
    }
#endif
    }
#else
#if RENDER == R_R4
    C.TessMethod = oTessellation.IDselected;
#endif

    // codepath is the same, only the shaders differ
    switch (C.iElement)
    {
    case SE_R2_NORMAL_HQ: // deffer
        uber_deffer(C, true, "base", "base", false, 0, true);

        C.r_Stencil(TRUE, D3DCMP_ALWAYS, 0xff, 0x7f, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
        C.r_StencilRef(0x01);
        C.r_End();
        break;
    case SE_R2_NORMAL_LQ: // deffer
        uber_deffer(C, false, "base", "base", false, 0, true);

        C.r_Stencil(TRUE, D3DCMP_ALWAYS, 0xff, 0x7f, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
        C.r_StencilRef(0x01);
        C.r_End();
        break;
    case SE_R2_SHADOW: // smap-direct
        // if (RImplementation.o.HW_smap)	C.r_Pass	("shadow_direct_base","dumb",	FALSE,TRUE,TRUE,FALSE);
        // else							C.r_Pass	("shadow_direct_base","shadow_direct_base",FALSE);
#if RENDER == R_R3
        C.r_Pass("shadow_direct_base", "dumb", FALSE, TRUE, TRUE, FALSE);
#elif RENDER == R_R4
        uber_shadow(C, "base");
#endif
        // C.r_Sampler		("s_base",C.L_textures[0]);
        C.r_dx11Texture("s_base", C.L_textures[0]);
        C.r_dx11Sampler("smp_base");
        C.r_dx11Sampler("smp_linear");
        C.r_ColorWriteEnable(false, false, false, false);
        C.r_End();
        break;
    }
#endif
}
} // namespace xray::render::RENDER_NAMESPACE
