#include "stdafx.h"
#include "olrR1Pipeline.h"

#if defined(XR_PLATFORM_ANDROID)
#include "xrEngine/CustomHUD.h"
#include "xrEngine/IGame_Persistent.h"
#include "Layers/xrRender/Blender.h"
#include "Layers/xrRender/Blender_Recorder.h"
#include "Layers/xrRender/DetailManager.h"
#include "Layers/xrRender/Shader.h"
#include "Layers/xrRender/olr_r1_mode.h"
#include "Layers/xrRenderGL/glBackendStateCache.h"
#include "olrR1Models.h"
#endif

namespace xray::render::RENDER_NAMESPACE::OlrR1Pipeline
{
namespace { unsigned deferredCalls = 0; }

void BeginFrame() { deferredCalls = 0; }

bool Independent()
{
#if defined(XR_PLATFORM_ANDROID)
    static const bool enabled = olr_r1_scene_enabled() && Core.Params &&
        strstr(Core.Params, "-olr_r1_independent");
    return enabled;
#else
    return false;
#endif
}

void EnterDeferredStage(const char* stage)
{
    // This guard is on the real entry points, rather than inferred from
    // resource names or a claimed zero in the profiling output.
    if (Independent())
    {
        ++deferredCalls;
        Msg("! [olr_r1_pipeline] forbidden R2 stage: %s calls=%u", stage, deferredCalls);
        FATAL(stage);
    }
}

void ReportFrame(unsigned unsupported, unsigned nativeDraws)
{
    if (!Independent()) return;
    static bool first = true;
    if (first || Device.dwFrame % 300 == 0)
    {
        Msg("* [olr_r1_pipeline] frame=%u independent=1 blocked=%u unsupported=%u native_draws=%u deferred_calls=%u",
            Device.dwFrame, unsigned(unsupported != 0), unsupported, nativeDraws, deferredCalls);
        first = false;
    }
}
}

#if defined(XR_PLATFORM_ANDROID)
namespace xray::render::RENDER_NAMESPACE
{
extern u32 g_r;
namespace
{
class R1GamePostprocess final : public IBlender
{
public:
    LPCSTR getComment() override { return "R1 shared game postprocess"; }
    void Compile(CBlender_Compile& C) override
    {
        IBlender::Compile(C);
        if (C.iElement != 0 && C.iElement != 4) return;
        C.r_Pass("stub_notransform_postpr", C.iElement == 4 ? "postprocess_cm" : "postprocess",
            FALSE, FALSE, FALSE, FALSE);
        C.r_Sampler_clf("s_base0", "$user$generic0");
        C.r_Sampler_clf("s_base1", "$user$generic0");
        C.r_Sampler("s_noise", "fx\\fx_noise2");
        if (C.iElement == 4)
        {
            C.r_Sampler_clf("s_grad0", "$user$cmap0");
            C.r_Sampler_clf("s_grad1", "$user$cmap1");
        }
        C.r_End();
    }
};

const char* ProgramName(const SPass* pass)
{
    if (!pass) return "unknown";
    if (pass->pp) return pass->pp->cName.c_str();
    if (pass->ps) return pass->ps->cName.c_str();
    if (pass->vs) return pass->vs->cName.c_str();
    return "fixed_state";
}

bool IndependentForward(const SPass* pass)
{
    // Only source-ported programs are admitted, never a generic R2 particle
    // program which may sample s_position for soft intersections.
    const char* name = ProgramName(pass);
    return xr_strcmp(name, "olr_r1_particle|olr_r1_particle|null") == 0 ||
        xr_strcmp(name, "olr_r1_particle_set|olr_r1_particle_set|null") == 0;
}

unsigned UnsupportedMaterials(R_dsgraph_structure& graph)
{
    xr_map<xr_string, unsigned> unsupported;
    unsigned total = 0, forward = 0;
    // Native static materials already have their own queue. Anything still
    // in these graphs must be migrated or explicitly proven shared-forward.
    for (unsigned priority = 0; priority != 2; ++priority)
        for (unsigned pass = 0; pass != SHADER_PASSES_MAX; ++pass)
        {
            xr_vector<R_dsgraph::mapNormal_T::value_type*> normals;
            graph.mapNormalPasses[priority][pass].get_any_p(normals);
            for (auto* group : normals)
            {
                const unsigned count = unsigned(group->second.size());
                if (IndependentForward(group->first)) { forward += count; continue; }
                unsupported[ProgramName(group->first)] += count;
                total += count;
            }
            xr_vector<R_dsgraph::mapMatrix_T::value_type*> models;
            graph.mapMatrixPasses[priority][pass].get_any_p(models);
            for (auto* group : models)
            {
                const unsigned count = unsigned(group->second.size());
                if (IndependentForward(group->first)) { forward += count; continue; }
                unsupported[ProgramName(group->first)] += count;
                total += count;
            }
        }
    const auto countQueue = [&](auto& queue, const char* family)
    {
        if (!queue.empty()) { ++unsupported[family]; ++total; }
    };
    countQueue(graph.mapHUD, "HUD/models: projected lighting and ROS");
    countQueue(graph.mapHUDSorted, "HUD blended models");
    xr_vector<R_dsgraph::mapSorted_T::value_type*> sorted;
    graph.mapSorted.get_any_p(sorted);
    for (auto* item : sorted)
    {
        const auto* element = item->second.se;
        bool ready = element && !element->passes.empty();
        if (element)
            for (const auto& pass : element->passes) ready &= IndependentForward(pass._get());
        if (ready) ++forward;
        else { ++unsupported["sorted forward: source material validation pending"]; ++total; }
    }
    countQueue(graph.mapEmissive, "emissive");
    countQueue(graph.mapHUDEmissive, "HUD emissive");
    countQueue(graph.mapWmark, "wallmark material");
    countQueue(graph.mapDistort, "distortion");
    if (RImplementation.Details && RImplementation.Details->HasR1DeferredMaterials())
    {
        ++unsupported["detail material without native shader"];
        ++total;
    }
    static bool reported = false;
    if (total && !reported)
    {
        Msg("* [olr_r1_coverage] independent_forward_queued=%u unsupported_draws_or_queues=%u", forward, total);
        for (const auto& item : unsupported)
            Msg("! [olr_r1_missing_material] draws_or_queues=%u family=%s", item.second, item.first.c_str());
        Msg("! Independent R1 is not ready for this scene. Returning to menu; no R2 fallback and no partial world frame.");
        reported = true;
    }
    return total;
}

void ClearFrameQueues(R_dsgraph_structure& graph)
{
    for (unsigned priority = 0; priority != 2; ++priority)
        for (unsigned pass = 0; pass != SHADER_PASSES_MAX; ++pass)
        {
            graph.mapNormalPasses[priority][pass].clear();
            graph.mapMatrixPasses[priority][pass].clear();
        }
    graph.mapOlrR1Lightmaps.clear();
    graph.mapHUD.clear();
    graph.mapHUDSorted.clear();
    graph.mapSorted.clear();
    graph.mapLOD.clear();
    graph.mapEmissive.clear();
    graph.mapHUDEmissive.clear();
    graph.mapWmark.clear();
    graph.mapDistort.clear();
}
}

void CRenderTarget::create_olr_r1_resources()
{
    const u32 width = Device.dwWidth, height = Device.dwHeight;
    rt_Base.resize(HW.BackBufferCount);
    for (u32 i = 0; i != HW.BackBufferCount; ++i)
    {
        string32 name;
        xr_sprintf(name, "%s%u", r2_RT_base, i);
        rt_Base[i].create(name, width, height, HW.Caps.fTarget, 1, {CRT::CreateBase});
    }
    rt_Base_Depth.create(r2_RT_base_depth, width, height, HW.Caps.fDepth, 1, {CRT::CreateBase});
    rt_MSAADepth = rt_Base_Depth;
    // Generic0 is the LDR scene; Generic1 is the shared menu/distortion image.
    // Neither is a G-buffer. No normal/position/albedo/light/HDR/shadow targets.
    rt_Generic_0.create(r2_RT_generic0, width, height, D3DFMT_A8R8G8B8);
    rt_Generic_1.create(r2_RT_generic1, width, height, D3DFMT_A8R8G8B8);
    rt_Generic_0_r = rt_Generic_0;
    rt_Generic_1_r = rt_Generic_1;
    R1GamePostprocess postprocess;
    s_postprocess.create(&postprocess, "olr_r1_game_postprocess");
    s_postprocess_msaa = s_postprocess;
    g_postprocess.create(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX3,
        RImplementation.Vertex.Buffer(), RImplementation.QuadIB);
    s_menu.create("distort");
    g_menu.create(FVF::F_TL, RImplementation.Vertex.Buffer(), RImplementation.QuadIB);
    for (unsigned i = 0; i != R__NUM_CONTEXTS; ++i)
    {
        dwWidth[i] = width;
        dwHeight[i] = height;
    }
    param_color_map_influence = param_color_map_interpolate = 0.f;
    const unsigned gbuffer = unsigned(bool(rt_Position)) + unsigned(bool(rt_Normal)) + unsigned(bool(rt_Color));
    const unsigned accumulation = unsigned(bool(rt_Accumulator)) + unsigned(bool(rt_Accumulator_temp));
    const unsigned shadows = unsigned(bool(rt_smap_depth)) + unsigned(bool(rt_smap_surf)) + unsigned(bool(rt_smap_rain));
    const unsigned exposure = unsigned(bool(rt_LUM_64)) + unsigned(bool(rt_LUM_8));
    const unsigned bloom = unsigned(bool(rt_Bloom_1)) + unsigned(bool(rt_Bloom_2));
    R_ASSERT(gbuffer + accumulation + shadows + exposure + bloom == 0);
    Msg("* [olr_r1_targets] LDR=RGBA8 scene=1 menu_distortion=1 shared_depth=1 Gbuffer=%u accumulator=%u shadow_atlas=%u luminance=%u bloom=%u",
        gbuffer, accumulation, shadows, exposure, bloom);
}

void CRenderTarget::destroy_olr_r1_resources()
{
    OlrR1Models::Destroy();
    if (r1_read_framebuffer)
        glDeleteFramebuffers(1, &r1_read_framebuffer);
    r1_read_framebuffer = 0;
    // All texture/geometry/shader ownership is the existing resource manager.
}

void CRenderTarget::present_olr_r1()
{
    if (u_need_PP())
    {
        phase_pp(); // shared game duality/noise/color effects, no R2 lighting
        return;
    }
    u_setrt(RCache, Device.dwWidth, Device.dwHeight, get_base_rt(), 0, 0, get_base_zb());
    if (!r1_read_framebuffer) glGenFramebuffers(1, &r1_read_framebuffer);
    // Separate read FBO avoids reading and writing the same attachment, and
    // avoids the desktop-only draw-buffer remapping rejected by GLES.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, r1_read_framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt_Generic_0->pRT, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    CHK_GL(glBlitFramebuffer(0, 0, Device.dwWidth, Device.dwHeight, 0, 0, Device.dwWidth, Device.dwHeight,
        GL_COLOR_BUFFER_BIT, GL_NEAREST));
    glBindFramebuffer(GL_FRAMEBUFFER, HW.pFB);
    GLHardwareState::Get().framebuffer.invalidate();
    GLHardwareState::InvalidateDrawBuffers();
    GLHardwareState::InvalidateAttachments();
}

void CRender::render_olr_r1()
{
    OlrR1Pipeline::BeginFrame();
    g_r = 1;
    auto& graph = get_imm_context();
    auto* menu = g_pGamePersistent ? g_pGamePersistent->m_pMainMenu : nullptr;
    if (!g_pGameLevel || (menu && menu->CanSkipSceneRendering()))
    {
        Target->u_setrt(RCache, Device.dwWidth, Device.dwHeight, Target->get_base_rt(), 0, 0, Target->get_base_zb());
        return;
    }
    if (m_bFirstFrameAfterReset) { m_bFirstFrameAfterReset = false; return; }
    r_main.sync(); // shared visibility/sector/portal preparation, not R2 lighting
    OlrR1Models::CalculateProjector();
    const unsigned unsupported = UnsupportedMaterials(graph);
    OlrR1Models::CaptureModels(graph);
    if (unsupported)
    {
        OlrR1Pipeline::ReportFrame(unsupported, 0);
        ClearFrameQueues(graph);
        if (menu) menu->Activate(true);
        return;
    }
    const u32 beforeDraws = RCache.stat.render.calls;
    Target->u_setrt(RCache, Target->rt_Generic_0, nullptr, nullptr, Target->rt_Base_Depth);
    RCache.ClearRT(Target->rt_Generic_0, color_rgba(0,0,0,255));
    RCache.ClearZB(Target->rt_Base_Depth, 1.f);
    RCache.set_Stencil(FALSE);
    RCache.set_CullMode(CULL_CCW);
    RCache.set_ColorWriteEnable();
    RCache.set_xform_view(Device.mView);
    RCache.set_xform_project(Device.mProject);
    graph.o.phase = PHASE_NORMAL;
    rmNormal(RCache);
    graph.render_olr_r1_lightmaps();
    graph.render_graph(0);
    if (Details) Details->RenderNativeR1(RCache);
    graph.render_lods(true, false);
    g_pGamePersistent->Environment().RenderSky();
    g_pGamePersistent->Environment().RenderClouds();
    graph.render_lods(false, true);
    graph.render_graph(1);
    graph.PortalTraverser.fade_render();
    graph.render_sorted();
    g_pGamePersistent->OnRenderPPUI_main();
    Target->present_olr_r1();
    OlrR1Pipeline::ReportFrame(0, RCache.stat.render.calls-beforeDraws);
}
}
#endif
