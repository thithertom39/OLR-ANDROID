#include "stdafx.h"
#pragma hdrstop

#include "LightTrack.h"
#include "xrEngine/IRenderable.h"

#if defined(USE_OGL)
#include "Layers/xrRenderGL/glTextureBindings.h"
#endif

#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
#include "xrEngine/AndroidSceneCompare.h"
#endif

#if defined(USE_DX11)
#include <DirectXMath.h>
#endif

namespace xray::render::RENDER_NAMESPACE
{
#if defined(USE_OGL)
bool CBackend::gles_state_rebind_enabled()
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    static const bool enabled = Core.Params && strstr(Core.Params, "-gles_state_rebind") != nullptr;
    return enabled;
#else
    return false;
#endif
}

namespace
{
void ClearGLTextureStage(u32 stage)
{
    GLTextureBindings::ClearStage(stage, CBackend::gles_state_rebind_enabled());
}
} // namespace
#endif

void CBackend::OnFrameEnd()
{
    if (!GEnv.isDedicatedServer)
    {
#if defined(USE_DX11)
        HW.get_context(CHW::IMM_CTX_ID)->ClearState();
#endif
        Invalidate();
    }
}

void CBackend::OnFrameBegin()
{
    if (!GEnv.isDedicatedServer)
    {
        PGO(Msg("PGO:*****frame[%d]*****", Device.dwFrame));

#ifndef USE_DX9
#ifdef USE_OGL
        // Presentation and context lifecycle operations may bind GL objects
        // directly. Establish physical state once at the next frame boundary.
        GLHardwareState::Invalidate();
        GLTextureBindings::Invalidate();
#endif
        Invalidate();
        // DX9 sets base rt and base zb by default
#ifndef USE_OGL
        // XXX: Getting broken HUD hands for OpenGL after calling rmNormal()
        RImplementation.rmNormal(*this);
#else
        set_FB(HW.pFB);
#endif
        set_RT(RImplementation.Target->get_base_rt());
        set_ZB(RImplementation.Target->get_base_zb());
#endif

        ZeroMemory(&stat, sizeof(stat));
        set_Stencil(FALSE);
    }
}

void CBackend::Invalidate()
{
    pRT[0] = 0;
    pRT[1] = 0;
    pRT[2] = 0;
    pRT[3] = 0;
    pZB = 0;
#if defined(USE_OGL)
    pFB = 0;
    pp = 0;
#endif

    decl = nullptr;
    vb = 0;
    ib = 0;
    vb_stride = 0;
#if defined(USE_OGL)
    cached_baseV = 0;
#endif

    state = nullptr;
    ps = 0;
    vs = 0;
    DX11_ONLY(gs = NULL);
#ifdef USE_DX11
    hs = 0;
    ds = 0;
    cs = 0;
#endif
    ctable = nullptr;

    T = nullptr;
    M = nullptr;
    C = nullptr;

    stencil_enable = u32(-1);
    stencil_func = u32(-1);
    stencil_ref = u32(-1);
    stencil_mask = u32(-1);
    stencil_writemask = u32(-1);
    stencil_fail = u32(-1);
    stencil_pass = u32(-1);
    stencil_zfail = u32(-1);
    cull_mode = u32(-1);
    fill_mode = u32(-1);
    z_enable = u32(-1);
    z_func = u32(-1);
    alpha_ref = u32(-1);
    colorwrite_mask = u32(-1);

    // Since constant buffers are unmapped (for DirecX 10)
    // transform setting handlers should be unmapped too.
    xforms.unmap();

#if defined(USE_DX11)
    m_pInputLayout = NULL;
    m_PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    m_bChangedRTorZB = false;
    m_pInputSignature = NULL;
    for (int i = 0; i < MaxCBuffers; ++i)
    {
        m_aPixelConstants[i] = 0;
        m_aVertexConstants[i] = 0;
        m_aGeometryConstants[i] = 0;
        m_aHullConstants[i] = 0;
        m_aDomainConstants[i] = 0;
        m_aComputeConstants[i] = 0;
    }
    StateManager.Reset();
    // Redundant call. Just no note that we need to unmap const
    // if we create dedicated class.
    StateManager.UnmapConstants();
    SRVSManager.ResetDeviceState();

    for (u32 gs_it = 0; gs_it < CTexture::mtMaxGeometryShaderTextures;)
        textures_gs[gs_it++] = 0;
    for (u32 hs_it = 0; hs_it < CTexture::mtMaxHullShaderTextures;)
        textures_hs[hs_it++] = 0;
    for (u32 ds_it = 0; ds_it < CTexture::mtMaxDomainShaderTextures;)
        textures_ds[ds_it++] = 0;
    for (u32 cs_it = 0; cs_it < CTexture::mtMaxComputeShaderTextures;)
        textures_cs[cs_it++] = 0;

    context_id = CHW::IMM_CTX_ID;
#endif // USE_DX11

    for (u32 ps_it = 0; ps_it < CTexture::mtMaxPixelShaderTextures;)
        textures_ps[ps_it++] = nullptr;
    for (u32 vs_it = 0; vs_it < CTexture::mtMaxVertexShaderTextures;)
        textures_vs[vs_it++] = nullptr;
    for (auto& matrix : matrices)
        matrix = nullptr;
}

void CBackend::set_ClipPlanes(u32 _enable, Fplane* _planes /*=NULL */, u32 count /* =0*/)
{
#if defined(USE_DX11) || defined(USE_OGL)
    // TODO: DX11: Implement in the corresponding vertex shaders
    // Use this to set up location, were shader setup code will get data
    // VERIFY(!"CBackend::set_ClipPlanes not implemented!");
    UNUSED(_enable);
    UNUSED(_planes);
    UNUSED(count);
    return;
#else
#   error No graphics API selected or enabled!
#endif
}

#ifndef DEDICATED_SREVER
void CBackend::set_ClipPlanes(u32 _enable, Fmatrix* _xform /*=NULL */, u32 fmask /* =0xff */)
{
    if (0 == HW.Caps.geometry.dwClipPlanes)
        return;
    if (!_enable)
    {
#if defined(USE_DX11) || defined(USE_OGL)
    // TODO: DX11: Implement in the corresponding vertex shaders
    // Use this to set up location, were shader setup code will get data
    // VERIFY(!"CBackend::set_ClipPlanes not implemented!");
#else
#   error No graphics API selected or enabled!
#endif
        return;
    }
    VERIFY(_xform && fmask);
    CFrustum F;
    F.CreateFromMatrix(*_xform, fmask);
    set_ClipPlanes(_enable, F.planes, F.p_count);
}

void CBackend::set_Textures(STextureList* textures_list)
{
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
    const bool force_rebind = gles_state_rebind_enabled();
    constexpr u32 StageCount = CTexture::rstVertex + CTexture::mtMaxVertexShaderTextures;
    std::array<bool, StageCount> used{};
    T = textures_list;

    if (textures_list)
    {
        for (auto& loader : *textures_list)
        {
            const u32 stage = loader.first;
            VERIFY(stage < StageCount);
            if (stage >= StageCount)
                continue;
            used[stage] = true;
            CTexture* texture = loader.second._get();
            CTexture*& local_texture = stage < CTexture::rstVertex ?
                textures_ps[stage] : textures_vs[stage - CTexture::rstVertex];
            if (force_rebind || local_texture != texture)
                stat.textures++;
            local_texture = texture;

            if (texture)
            {
                // Always visit the delegate: an alias can change its GLuint
                // without changing CTexture*, and animated textures update here.
                // The physical binding cache eliminates redundant GL calls.
                texture->bind(*this, stage);
                if (stage < CTexture::rstVertex)
                    texture->last_slice = texture->curr_slice;
            }
            else
                GLTextureBindings::ClearStage(stage, force_rebind);
        }
    }

    // Sparse lists can leave holes below their highest occupied slot. Clear
    // those slots too, but only issue GL calls for nonzero/unknown bindings.
#if defined(XR_PLATFORM_ANDROID)
    static const bool prune_empty_stages =
        AndroidSceneCompare::HasOption(Core.Params, "-gles_prune_empty_texture_stages");
    const u64 stages_to_clear = prune_empty_stages && !force_rebind ?
        GLTextureBindings::StagesToClear() : ~u64{0};
#endif
    for (u32 stage = 0; stage < StageCount; ++stage)
    {
        if (used[stage])
            continue;
        if (stage < CTexture::rstVertex)
            textures_ps[stage] = nullptr;
        else
            textures_vs[stage - CTexture::rstVertex] = nullptr;
#if defined(XR_PLATFORM_ANDROID)
        if (!(stages_to_clear & (u64{1} << stage)))
            continue;
#endif
        GLTextureBindings::ClearStage(stage, force_rebind);
    }
#else
#if defined(USE_OGL)
    const bool force_rebind = gles_state_rebind_enabled();
    bool used_ps[CTexture::mtMaxPixelShaderTextures]{};
    bool used_vs[CTexture::mtMaxVertexShaderTextures]{};
#else
    constexpr bool force_rebind = false;
#endif
    // TODO: expose T invalidation method
    //if (T == textures_list) // disabled due to cases when the set of resources the same, but different srv is need to be bind
    //    return;
    T = textures_list;
    // If resources weren't set at all we should clear from resource #0.
    int _last_ps = -1;
    int _last_vs = -1;
#if defined(USE_DX11)
    int _last_gs = -1;
    int _last_hs = -1;
    int _last_ds = -1;
    int _last_cs = -1;
#endif
    auto it = textures_list->begin();
    const auto end = textures_list->end();

    for (; it != end; ++it)
    {
        std::pair<u32, ref_texture>& loader = *it;
        u32 load_id = loader.first;
        CTexture* load_surf = loader.second._get();
        //if (load_id < 256) {
        if (load_id < CTexture::rstVertex)
        {
            // Set up pixel shader resources
            VERIFY(load_id < CTexture::mtMaxPixelShaderTextures);
#if defined(USE_OGL)
            if (force_rebind)
                used_ps[load_id] = true;
#endif
            // ordinary pixel surface
            if ((int)load_id > _last_ps)
                _last_ps = load_id;
            if (force_rebind || textures_ps[load_id] != load_surf || (load_surf && (load_surf->last_slice != load_surf->curr_slice)))
            {
                textures_ps[load_id] = load_surf;
                stat.textures++;

                if (load_surf)
                {
                    PGO(Msg("PGO:tex%d:%s", load_id, load_surf->cName.c_str()));
                    load_surf->bind(*this, load_id);
                    //load_surf->Apply(load_id);
                    load_surf->last_slice = load_surf->curr_slice;
                }
#if defined(USE_OGL)
                else if (force_rebind)
                    ClearGLTextureStage(load_id);
#endif
            }
        }
        else
#if defined(USE_DX11)
        if (load_id < CTexture::rstGeometry)
#endif
        {
            // Set up pixel shader resources
            VERIFY(load_id < CTexture::rstVertex + CTexture::mtMaxVertexShaderTextures);

            // vertex only //d-map or vertex
            u32 load_id_remapped = load_id - CTexture::rstVertex;
#if defined(USE_OGL)
            if (force_rebind)
                used_vs[load_id_remapped] = true;
#endif
            if ((int)load_id_remapped > _last_vs)
                _last_vs = load_id_remapped;
            if (force_rebind || textures_vs[load_id_remapped] != load_surf)
            {
                textures_vs[load_id_remapped] = load_surf;
                stat.textures++;

                if (load_surf)
                {
                    PGO(Msg("PGO:tex%d:%s", load_id, load_surf->cName.c_str()));
                    load_surf->bind(*this, load_id);
                    //load_surf->Apply(load_id);
                }
#if defined(USE_OGL)
                else if (force_rebind)
                    ClearGLTextureStage(load_id);
#endif
            }
        }
#if defined(USE_DX11)
        else if (load_id < CTexture::rstHull)
        {
            // Set up pixel shader resources
            VERIFY(load_id < CTexture::rstGeometry + CTexture::mtMaxGeometryShaderTextures);

            // vertex only //d-map or vertex
            u32 load_id_remapped = load_id - CTexture::rstGeometry;
            if ((int)load_id_remapped > _last_gs)
                _last_gs = load_id_remapped;
            if (textures_gs[load_id_remapped] != load_surf)
            {
                textures_gs[load_id_remapped] = load_surf;
                stat.textures++;

                if (load_surf)
                {
                    PGO(Msg("PGO:tex%d:%s", load_id, load_surf->cName.c_str()));
                    load_surf->bind(*this, load_id);
                    //load_surf->Apply(load_id);
                }
            }
        }
        else if (load_id < CTexture::rstDomain)
        {
            //  Set up pixel shader resources
            VERIFY(load_id < CTexture::rstHull + CTexture::mtMaxHullShaderTextures);

            // vertex only //d-map or vertex
            u32 load_id_remapped = load_id - CTexture::rstHull;
            if ((int)load_id_remapped > _last_hs)
                _last_hs = load_id_remapped;
            if (textures_hs[load_id_remapped] != load_surf)
            {
                textures_hs[load_id_remapped] = load_surf;
                stat.textures++;

                if (load_surf)
                {
                    PGO(Msg("PGO:tex%d:%s", load_id, load_surf->cName.c_str()));
                    load_surf->bind(*this, load_id);
                    //load_surf->Apply(load_id);
                }
            }
        }
        else if (load_id < CTexture::rstCompute)
        {
            // Set up pixel shader resources
            VERIFY(load_id < CTexture::rstDomain + CTexture::mtMaxDomainShaderTextures);

            // vertex only //d-map or vertex
            u32 load_id_remapped = load_id - CTexture::rstDomain;
            if ((int)load_id_remapped > _last_ds)
                _last_ds = load_id_remapped;
            if (textures_ds[load_id_remapped] != load_surf)
            {
                textures_ds[load_id_remapped] = load_surf;
                stat.textures++;

                if (load_surf)
                {
                    PGO(Msg("PGO:tex%d:%s", load_id, load_surf->cName.c_str()));
                    load_surf->bind(*this, load_id);
                    //load_surf->Apply(load_id);
                }
            }
        }
        else if (load_id < CTexture::rstInvalid)
        {
            // Set up pixel shader resources
            VERIFY(load_id < CTexture::rstCompute + CTexture::mtMaxComputeShaderTextures);

            // vertex only //d-map or vertex
            u32 load_id_remapped = load_id - CTexture::rstCompute;
            if ((int)load_id_remapped > _last_cs)
                _last_cs = load_id_remapped;
            if (textures_cs[load_id_remapped] != load_surf)
            {
                textures_cs[load_id_remapped] = load_surf;
                stat.textures++;

                if (load_surf)
                {
                    PGO(Msg("PGO:tex%d:%s", load_id, load_surf->cName.c_str()));
                    load_surf->bind(*this, load_id);
                    //load_surf->Applyload_id);
                }
            }
        }
        else
        {
            VERIFY("Invalid enum");
        }
#endif // USE_DX11
    }

#if defined(USE_OGL)
    if (force_rebind)
    {
        // Another command list may have populated any stage, including gaps in
        // this pass's sparse list. Local nullptr entries do not prove GL is clear.
        for (u32 stage = 0; stage < CTexture::mtMaxPixelShaderTextures; ++stage)
        {
            if (used_ps[stage])
                continue;
            textures_ps[stage] = nullptr;
            ClearGLTextureStage(stage);
        }
        for (u32 stage = 0; stage < CTexture::mtMaxVertexShaderTextures; ++stage)
        {
            if (used_vs[stage])
                continue;
            textures_vs[stage] = nullptr;
            ClearGLTextureStage(CTexture::rstVertex + stage);
        }
        return;
    }
#endif

    // clear remaining stages (PS)
    for (++_last_ps; _last_ps < CTexture::mtMaxPixelShaderTextures; _last_ps++)
    {
        if (!textures_ps[_last_ps])
            continue;

        textures_ps[_last_ps] = nullptr;
#if defined(USE_DX11)
        // TODO: DX11: Optimise: set all resources at once
        ID3DShaderResourceView* pRes = 0;
        // HW.pDevice->PSSetShaderResources(_last_ps, 1, &pRes);
        SRVSManager.SetPSResource(_last_ps, pRes);
#elif defined(USE_OGL)
        ClearGLTextureStage(_last_ps);
#else
#   error No graphics API selected or enabled!
#endif
    }
    // clear remaining stages (VS)
    for (++_last_vs; _last_vs < CTexture::mtMaxVertexShaderTextures; _last_vs++)
    {
        if (!textures_vs[_last_vs])
            continue;

        textures_vs[_last_vs] = nullptr;
#if defined(USE_DX11)
        // TODO: DX11: Optimise: set all resources at once
        ID3DShaderResourceView* pRes = 0;
        // HW.pDevice->VSSetShaderResources(_last_vs, 1, &pRes);
        SRVSManager.SetVSResource(_last_vs, pRes);
#elif defined(USE_OGL)
        ClearGLTextureStage(CTexture::rstVertex + _last_vs);
#else
#   error No graphics API selected or enabled!
#endif
    }

#if defined(USE_DX11)
    // clear remaining stages (VS)
    for (++_last_gs; _last_gs < CTexture::mtMaxGeometryShaderTextures; _last_gs++)
    {
        if (!textures_gs[_last_gs])
            continue;

        textures_gs[_last_gs] = 0;

        // TODO: DX11: Optimise: set all resources at once
        ID3DShaderResourceView* pRes = 0;
        // HW.pDevice->GSSetShaderResources(_last_gs, 1, &pRes);
        SRVSManager.SetGSResource(_last_gs, pRes);
    }
    for (++_last_hs; _last_hs < CTexture::mtMaxHullShaderTextures; _last_hs++)
    {
        if (!textures_hs[_last_hs])
            continue;

        textures_hs[_last_hs] = 0;

        // TODO: DX11: Optimise: set all resources at once
        ID3DShaderResourceView* pRes = 0;
        SRVSManager.SetHSResource(_last_hs, pRes);
    }
    for (++_last_ds; _last_ds < CTexture::mtMaxDomainShaderTextures; _last_ds++)
    {
        if (!textures_ds[_last_ds])
            continue;

        textures_ds[_last_ds] = 0;

        // TODO: DX11: Optimise: set all resources at once
        ID3DShaderResourceView* pRes = 0;
        SRVSManager.SetDSResource(_last_ds, pRes);
    }
    for (++_last_cs; _last_cs < CTexture::mtMaxComputeShaderTextures; _last_cs++)
    {
        if (!textures_cs[_last_cs])
            continue;

        textures_cs[_last_cs] = 0;

        // TODO: DX11: Optimise: set all resources at once
        ID3DShaderResourceView* pRes = 0;
        SRVSManager.SetCSResource(_last_cs, pRes);
    }
#endif // USE_DX11
#endif // Android/GLES physical texture bindings
}
#else

void CBackend::set_ClipPlanes(u32 _enable, Fmatrix* _xform /*=NULL */, u32 fmask /* =0xff */) {}
void CBackend::set_Textures(STextureList* textures_list) {}

#endif // DEDICATED SERVER

void CBackend::SetupStates()
{
    set_CullMode(CULL_CCW);
#if defined(USE_DX11)
    SSManager.SetMaxAnisotropy(ps_r__tf_Anisotropic);
    SSManager.SetMipLODBias(ps_r__tf_Mipbias);
#elif defined(USE_OGL)
    // TODO: OGL: Implement SetupStates().
#else
#   error No graphics API selected or enabled!
#endif
}


// Device dependance
void CBackend::OnDeviceCreate()
{
    ZoneScoped;

#if defined(USE_DX11)
    HW.get_context(context_id)->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(&pAnnotation));
#endif

    // Debug Draw
    InitializeDebugDraw();

    // invalidate caching
    Invalidate();
}

void CBackend::OnDeviceDestroy()
{
    // Debug Draw
    DestroyDebugDraw();

#if defined(USE_DX11)
    //  Destroy state managers
    StateManager.Reset();
#endif

#if defined(USE_DX11)
    _RELEASE(pAnnotation);
#endif
}

void CBackend::apply_lmaterial()
{
    R_constant* C = get_c(c_sbase)._get(); // get sampler
    if (!C)
        return;

    VERIFY(RC_dest_sampler == C->destination);
#if defined(USE_DX11)
    VERIFY(RC_dx11texture == C->type);
#elif defined(USE_OGL)
    VERIFY(RC_sampler == C->type);
#else
#   error No graphics API selected or enabled!
#endif

    CTexture* T = get_ActiveTexture(u32(C->samp.index));
    VERIFY(T);
    float mtl = T->m_material;
#ifdef DEBUG
    if (ps_r2_ls_flags.test(R2FLAG_GLOBALMATERIAL))
        mtl = ps_r2_gmaterial;
#endif
    hemi.set_material(o_hemi, o_sun, 0, (mtl + .5f) / 4.f);
    hemi.set_pos_faces(o_hemi_cube[CROS_impl::CUBE_FACE_POS_X],
                                o_hemi_cube[CROS_impl::CUBE_FACE_POS_Y],
                                o_hemi_cube[CROS_impl::CUBE_FACE_POS_Z]);
    hemi.set_neg_faces(o_hemi_cube[CROS_impl::CUBE_FACE_NEG_X],
                                o_hemi_cube[CROS_impl::CUBE_FACE_NEG_Y],
                                o_hemi_cube[CROS_impl::CUBE_FACE_NEG_Z]);
}
} // namespace xray::render::RENDER_NAMESPACE
