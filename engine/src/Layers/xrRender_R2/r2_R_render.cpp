#include "stdafx.h"

#include "xrCore/Threading/TaskManager.hpp"

#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/CustomHUD.h"
#include "xrEngine/xr_object.h"
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
#include "xrEngine/AndroidSceneCompare.h"
#endif

#include "Layers/xrRender/FBasicVisual.h"
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "Layers/xrRenderGL/glGpuProfiler.h"
#endif

namespace xray::render::RENDER_NAMESPACE
{
void CRender::RenderMenu()
{
#if defined(USE_DX11)
    TracyD3D11Zone(HW.profiler_ctx, "render_menu");
#endif
    PIX_EVENT(render_menu);
#if defined(USE_OGL)
    // Binding a smaller FBO does not update the GL viewport. The menu runs
    // without the scene's rmNormal(), so establish the actual render size.
    RCache.SetViewport({0, 0, Device.dwWidth, Device.dwHeight, 0.f, 1.f});
    static bool viewport_logged = false;
    if (!viewport_logged && Core.Params && strstr(Core.Params, "-gles_shadow_audit"))
    {
        viewport_logged = true;
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        Msg("* [gles_menu_audit] render=%ux%u viewport=%d,%d,%d,%d",
            Device.dwWidth, Device.dwHeight, viewport[0], viewport[1], viewport[2], viewport[3]);
    }
#endif
    //	Globals
    RCache.set_CullMode(CULL_CCW);
    RCache.set_Stencil(FALSE);
    RCache.set_ColorWriteEnable();

    // Main Render
    {
        Target->u_setrt(RCache, Target->rt_Generic_0, nullptr, nullptr, Target->rt_Base_Depth); // LDR RT
        g_pGamePersistent->OnRenderPPUI_main(); // PP-UI
    }
    // Distort
    {
        Target->u_setrt(RCache, Target->rt_Generic_1, nullptr, nullptr, Target->rt_Base_Depth); // Now RT is a distortion mask
        RCache.ClearRT(Target->rt_Generic_1, color_rgba(127, 127, 0, 127));
        g_pGamePersistent->OnRenderPPUI_PP(); // PP-UI
    }

    // Actual Display
    Target->u_setrt(RCache, Device.dwWidth, Device.dwHeight, Target->get_base_rt(), 0, 0, Target->get_base_zb());
    RCache.set_Shader(Target->s_menu);
    RCache.set_Geometry(Target->g_menu);

    Fvector2 p0, p1;
    u32 Offset;
    u32 C = color_rgba(255, 255, 255, 255);
    float _w = float(Device.dwWidth);
    float _h = float(Device.dwHeight);
    float d_Z = EPS_S;
    float d_W = 1.f;
    p0.set(.5f / _w, .5f / _h);
    p1.set((_w + .5f) / _w, (_h + .5f) / _h);

    FVF::TL* pv = (FVF::TL*)RImplementation.Vertex.Lock(4, Target->g_menu->vb_stride, Offset);
#if defined(USE_DX11)
    pv->set(EPS, float(_h + EPS), d_Z, d_W, C, p0.x, p1.y);
    pv++;
    pv->set(EPS, EPS, d_Z, d_W, C, p0.x, p0.y);
    pv++;
    pv->set(float(_w + EPS), float(_h + EPS), d_Z, d_W, C, p1.x, p1.y);
    pv++;
    pv->set(float(_w + EPS), EPS, d_Z, d_W, C, p1.x, p0.y);
    pv++;
#elif defined(USE_OGL)
    pv->set(EPS, EPS, d_Z, d_W, C, p0.x, p0.y);
    pv++;
    pv->set(EPS, float(_h + EPS), d_Z, d_W, C, p0.x, p1.y);
    pv++;
    pv->set(float(_w + EPS), EPS, d_Z, d_W, C, p1.x, p0.y);
    pv++;
    pv->set(float(_w + EPS), float(_h + EPS), d_Z, d_W, C, p1.x, p1.y);
    pv++;
#else
#   error No graphics API selected or enabled!
#endif
    RImplementation.Vertex.Unlock(4, Target->g_menu->vb_stride);
    RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
}

extern u32 g_r;
void CRender::Render()
{
    ZoneScoped;
#if defined(USE_DX11)
    TracyD3D11Zone(HW.profiler_ctx, "Render");
#endif
    PIX_EVENT(CRender_Render);

    g_r = 1;

    rmNormal(RCache);

    IMainMenu* pMainMenu = g_pGamePersistent ? g_pGamePersistent->m_pMainMenu : 0;
    bool bMenu = pMainMenu ? pMainMenu->CanSkipSceneRendering() : false;

    // XXX: do we need to handle case when there is level, but HUD isn't loaded yet?
    // if (!(g_pGameLevel && g_hud) || bMenu)
    if (!g_pGameLevel || bMenu)
    {
        Target->u_setrt(RCache, Device.dwWidth, Device.dwHeight, Target->get_base_rt(), 0, 0, Target->get_base_zb());
        return;
    }

    if (m_bFirstFrameAfterReset)
    {
        m_bFirstFrameAfterReset = false;
        return;
    }

    //.	VERIFY					(g_pGameLevel && g_pGameLevel->pHUD);
    auto& dsgraph = get_imm_context();
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    const bool compareScene = Device.dwFrame % 67 == 0 && g_pGameLevel->bReady && !Device.Paused() &&
        !Device.dwPrecacheFrame && AndroidSceneCompare::Enabled(Core.Params);
    CTimer compareTimer;
    const u32 atlasStart = Stats.s_used;
    const u32 shadowPassStart = Stats.s_merged;
    const u32 casterStart = Stats.ic_total;
#endif
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope worldScope(GLGpuProfiler::Stage::World);
#endif

    //******* Z-prefill calc - DEFERRER RENDERER
    if (ps_r2_ls_flags.test(R2FLAG_ZFILL))
    {
        PIX_EVENT(DEFER_Z_FILL);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope zPrefillScope(GLGpuProfiler::Stage::ZPrefill);
#endif
        BasicStats.Culling.Begin();
        float z_distance = ps_r2_zfill;
        Fmatrix m_zfill, m_project;
        m_project.build_projection(deg2rad(Device.fFOV /* *Device.fASPECT*/), Device.fASPECT, VIEWPORT_NEAR,
            z_distance * g_pGamePersistent->Environment().CurrentEnv.far_plane);
        m_zfill.mul(m_project, Device.mView);

        if (last_sector_id != IRender_Sector::INVALID_SECTOR_ID)
        {
            dsgraph.o.phase = PHASE_SMAP;
            dsgraph.r_pmask(true, false); // enable priority "0"
            dsgraph.set_Recorder(nullptr);
            dsgraph.o.use_hom = true;
            dsgraph.o.is_main_pass = true;
            dsgraph.o.sector_id = last_sector_id;
            dsgraph.o.portal_traverse_flags = CPortalTraverser::VQ_HOM | CPortalTraverser::VQ_SSA | CPortalTraverser::VQ_FADE;
            dsgraph.o.spatial_traverse_flags = ISpatial_DB::O_ORDERED;
            dsgraph.o.spatial_types = STYPE_RENDERABLE | STYPE_LIGHTSOURCE;
            dsgraph.o.view_pos = Device.vCameraPosition;
            dsgraph.o.xform = m_zfill;
            dsgraph.o.view_frustum = ViewBase;
            dsgraph.o.query_box_side = VIEWPORT_NEAR + EPS_L;
            dsgraph.o.precise_portals = true;

            dsgraph.build_subspace();
        }
        BasicStats.Culling.End();
    }

    //*******
    // Sync point
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope syncScope(GLGpuProfiler::Stage::SyncWait);
#endif
    BasicStats.WaitS.Begin();
    {
        q_sync_point.Wait(ps_r2_wait_sleep, ps_r2_wait_timeout);
    }
    BasicStats.WaitS.End();
    q_sync_point.End();

#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    {
        GLGpuProfiler::Scope mainVisibility(GLGpuProfiler::Stage::MainVisibilityWait, false);
        r_main.sync();
    }
#else
    r_main.sync();
#endif
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    syncScope.Stop();
    GLGpuProfiler::Scope prepareScope(GLGpuProfiler::Stage::ScenePrepare);
#endif

    if (ps_r2_ls_flags.test(R2FLAG_ZFILL))
    {
        // flush
        Target->phase_scene_prepare();
        dsgraph.cmd_list.set_ColorWriteEnable(FALSE);
        dsgraph.render_graph(0);
        dsgraph.cmd_list.set_ColorWriteEnable();
    }
    else
    {
        Target->phase_scene_prepare();
    }
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    prepareScope.Stop();
#endif

    BOOL split_the_scene_to_minimize_wait = FALSE;
    if (ps_r2_ls_flags.test(R2FLAG_EXP_SPLIT_SCENE))
        split_the_scene_to_minimize_wait = TRUE;

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    const u32 mainDrawStart = dsgraph.cmd_list.stat.render.calls;
#endif
    //******* Main render :: PART-0	-- first
#ifdef USE_OGL
    if (psDeviceFlags.test(rsWireframe))
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif
    if (!split_the_scene_to_minimize_wait)
    {
        PIX_EVENT(DEFER_PART0_NO_SPLIT);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope geometryScope(GLGpuProfiler::Stage::MainGeometry);
#endif
        // level, DO NOT SPLIT
        Target->phase_scene_begin();
        dsgraph.render_hud();
        dsgraph.render_graph(0);
        dsgraph.render_lods(true, true);
        if (Details)
            Details->Render(dsgraph.cmd_list);
        Target->phase_scene_end();
    }
    else
    {
        PIX_EVENT(DEFER_PART0_SPLIT);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope geometryScope(GLGpuProfiler::Stage::MainGeometry);
#endif
        // level, SPLIT
        Target->phase_scene_begin();
        dsgraph.render_graph(0);
        Target->disable_aniso();
    }
#ifdef USE_OGL
    if (psDeviceFlags.test(rsWireframe))
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    //******* Occlusion testing of volume-limited light-sources
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope visibilityScope(GLGpuProfiler::Stage::LightVisibility);
#endif
    Target->phase_occq();
    LP_normal.clear();
    LP_pending.clear();
    if (o.msaa)
    {
#if defined(USE_DX11)
        dsgraph.cmd_list.set_ZB(Target->rt_MSAADepth->pZRT[dsgraph.cmd_list.context_id]);
#elif defined(USE_OGL)
        dsgraph.cmd_list.set_ZB(Target->rt_MSAADepth->pZRT);
#endif
    }
    {
        PIX_EVENT(DEFER_TEST_LIGHT_VIS);
        light_Package& LP = Lights.package;

        // stats
        Stats.l_shadowed = LP.v_shadowed.size();
        Stats.l_unshadowed = LP.v_point.size() + LP.v_spot.size();
        Stats.l_total = Stats.l_shadowed + Stats.l_unshadowed;

        // perform tests
        size_t count = 0;
        count = _max(count, LP.v_point.size());
        count = _max(count, LP.v_spot.size());
        count = _max(count, LP.v_shadowed.size());
        for (size_t it = 0; it < count; it++)
        {
            if (it < LP.v_point.size())
            {
                light* L = LP.v_point[it];
                L->vis_prepare(dsgraph.cmd_list);
                if (L->vis.pending)
                    LP_pending.v_point.push_back(L);
                else
                    LP_normal.v_point.push_back(L);
            }
            if (it < LP.v_spot.size())
            {
                light* L = LP.v_spot[it];
                L->vis_prepare(dsgraph.cmd_list);
                if (L->vis.pending)
                    LP_pending.v_spot.push_back(L);
                else
                    LP_normal.v_spot.push_back(L);
            }
            if (it < LP.v_shadowed.size())
            {
                light* L = LP.v_shadowed[it];
                L->vis_prepare(dsgraph.cmd_list);
                if (L->vis.pending)
                    LP_pending.v_shadowed.push_back(L);
                else
                    LP_normal.v_shadowed.push_back(L);
            }
        }
    }
    LP_normal.sort();
    LP_pending.sort();
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    visibilityScope.Stop();
#endif

    //******* Main render :: PART-1 (second)
    if (split_the_scene_to_minimize_wait)
    {
        PIX_EVENT(DEFER_PART1_SPLIT);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope geometryScope(GLGpuProfiler::Stage::MainGeometry);
#endif
        // skybox can be drawn here
        if (false)
        {
            Target->u_setrt(dsgraph.cmd_list, Target->rt_Generic_0_r, Target->rt_Generic_1_r, nullptr, Target->rt_MSAADepth);
            dsgraph.cmd_list.set_CullMode(CULL_NONE);
            dsgraph.cmd_list.set_Stencil(FALSE);

            // draw skybox
            dsgraph.cmd_list.set_ColorWriteEnable();
            dsgraph.cmd_list.set_Z(false);
            g_pGamePersistent->Environment().RenderSky();
            dsgraph.cmd_list.set_Z(true);
        }

        // level
        Target->phase_scene_begin();
        dsgraph.render_hud();
        dsgraph.render_lods(true, true);
        if (Details)
            Details->Render(dsgraph.cmd_list);
        Target->phase_scene_end();
    }

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    const u32 mainDraws = dsgraph.cmd_list.stat.render.calls - mainDrawStart;
    if (HWOCC.is_light_enabled() && !o.msaa && !LP_normal.v_shadowed.empty())
    {
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope visibilityProbe(GLGpuProfiler::Stage::LightVisibility);
#endif
        // On split scenes complete HUD/LOD/details before sampling this depth.
        if (split_the_scene_to_minimize_wait)
            Target->phase_occq();
        HWOCC.occq_light_issue(dsgraph.cmd_list, LP_normal.v_shadowed);
    }
#endif
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope wallmarksScope(GLGpuProfiler::Stage::Wallmarks);
#endif
    if (g_pGameLevel->pHUD && g_pGameLevel->pHUD->RenderActiveItemUIQuery())
    {
        Target->phase_wallmarks();
        dsgraph.render_hud_ui();
    }

    // Wall marks
    if (Wallmarks)
    {
        PIX_EVENT(DEFER_WALLMARKS);
        Target->phase_wallmarks();
        g_r = 0;
        Wallmarks->Render(); // wallmarks has priority as normal geometry
    }
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    wallmarksScope.Stop();
#endif

    // Update incremental shadowmap-visibility solver
    {
        PIX_EVENT(DEFER_FLUSH_OCCLUSION);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope lightFlushScope(GLGpuProfiler::Stage::LightFlush);
#endif
        u32 it = 0;
        for (it = 0; it < Lights_LastFrame.size(); it++)
        {
            if (0 == Lights_LastFrame[it])
                continue;
            try
            {
                for (int id = 0; id < 3; ++id)
                    Lights_LastFrame[it]->svis[id].flushoccq();
            }
            catch (...)
            {
                Msg("! Failed to flush-OCCq on light [%d] %X", it, *(u32*)(&Lights_LastFrame[it]));
            }
        }
        Lights_LastFrame.clear();
    }

    // full screen pass to mark msaa-edge pixels in highest stencil bit
    if (o.msaa)
    {
        PIX_EVENT(MARK_MSAA_EDGES);
        Target->mark_msaa_edges();
    }

#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope rainScope(GLGpuProfiler::Stage::Rain);
#endif
    r_rain.sync();
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    rainScope.Stop();
#endif

    // Directional light - fucking sun
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    if (compareScene)
        compareTimer.Start();
#endif
    {
        PIX_EVENT(DEFER_SUN);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope sunScope(GLGpuProfiler::Stage::SunShadowAndDirect);
#endif
        Stats.l_visible++;
        if (!RImplementation.o.oldshadowcascades)
            r_sun.sync();
        else
            r_sun_old.sync();
        Target->accum_direct_blend(dsgraph.cmd_list);
    }
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    const float sunMs = compareScene ? compareTimer.GetElapsed_sec() * 1000.f : 0.f;
#endif

    {
        PIX_EVENT(DEFER_SELF_ILLUM);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        // The accumulator state setup also runs in frames with no emissive draws.
        const bool hasEmissiveDraws = !dsgraph.mapEmissive.empty() || !dsgraph.mapHUDEmissive.empty();
        GLGpuProfiler::Scope emissiveScope(GLGpuProfiler::Stage::Emissive, hasEmissiveDraws);
#endif
        Target->phase_accumulator(dsgraph.cmd_list);
        // Render emissive geometry, stencil - write 0x0 at pixel pos
        dsgraph.cmd_list.set_xform_project(Device.mProject);
        dsgraph.cmd_list.set_xform_view(Device.mView);
        // Stencil - write 0x1 at pixel pos -
        if (!o.msaa)
        {
            dsgraph.cmd_list.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0xff,
                D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
        }
        else
        {
            dsgraph.cmd_list.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0x7f,
                D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
        }
        dsgraph.cmd_list.set_CullMode(CULL_CCW);
        dsgraph.cmd_list.set_ColorWriteEnable();
        dsgraph.render_emissive();
    }

    // Lighting, non dependant on OCCQ
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    if (compareScene)
        compareTimer.Start();
    if (HWOCC.is_light_enabled())
        HWOCC.occq_light_collect(LP_normal.v_shadowed, compareScene);
#endif
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::BeginLocalLightsElapsed();
#endif
    {
        PIX_EVENT(DEFER_LIGHT_NO_OCCQ);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope lightsScope(GLGpuProfiler::Stage::LocalLights);
#endif
        {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalNormal);
#endif
    render_lights(LP_normal);
}
    }

    // Lighting, dependant on OCCQ
    {
        PIX_EVENT(DEFER_LIGHT_OCCQ);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope lightsScope(GLGpuProfiler::Stage::LocalLights);
#endif
        {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalPending);
#endif
    render_lights(LP_pending);
}
    }

    // Postprocess
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::EndLocalLightsElapsed();
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    const float localMs = compareScene ? compareTimer.GetElapsed_sec() * 1000.f : 0.f;
#endif
    {
        PIX_EVENT(DEFER_LIGHT_COMBINE);
        Target->phase_combine();
    }

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    if (compareScene)
    {
        const auto& dir = Device.vCameraDirection;
        Msg("* [gles_scene_compare] frame=%u dir=%.5f,%.5f,%.5f main_sectors=%zu spatial_candidates=%u sector_queries=%u"
            " main_draws=%u shadow_candidates=%u unshadowed_candidates=%u shadow_atlases=%u shadow_passes=%u"
            " static_caster_entries=%u hwocc=%u legacy_sector_queries=%u sun_wall_ms=%.3f local_wall_ms=%.3f",
            Device.dwFrame, double(dir.x), double(dir.y), double(dir.z), dsgraph.PortalTraverser.r_sectors.size(),
            dsgraph.main_spatial_candidates, dsgraph.main_sector_queries, mainDraws, Stats.l_shadowed,
            Stats.l_unshadowed, Stats.s_used - atlasStart, Stats.s_merged - shadowPassStart,
            Stats.ic_total - casterStart, unsigned(HWOCC.is_enabled()), unsigned(dsgraph.legacy_sector_queries),
            double(sunMs), double(localMs));
    }
#endif
    VERIFY(dsgraph.mapDistort.empty());
}

void CRender::render_forward()
{
    ZoneScoped;
    auto& dsgraph = get_imm_context();

    //******* Main render - second order geometry (the one, that doesn't support deffering)
    //.todo: should be done inside "combine" with estimation of of luminance, tone-mapping, etc.
    {
        //	Igor: we don't want to render old lods on next frame.
        dsgraph.mapLOD.clear();
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
        dsgraph.render_olr_r1_lightmaps();
#endif
        dsgraph.render_graph(1); // normal level, secondary priority
        dsgraph.PortalTraverser.fade_render(); // faded-portals
        dsgraph.render_sorted(); // strict-sorted geoms
        g_pGamePersistent->Environment().RenderLast(); // rain/thunder-bolts
    }
}

// Перед началом рендера мира --#SM+#--
void CRender::BeforeWorldRender() {}

// После рендера мира и пост-эффектов --#SM+#--
void CRender::AfterWorldRender() {}
} // namespace xray::render::RENDER_NAMESPACE
