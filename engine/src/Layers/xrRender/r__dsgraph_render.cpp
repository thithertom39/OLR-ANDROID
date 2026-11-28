#include "stdafx.h"

#include "xrEngine/IRenderable.h"
#include "xrEngine/CustomHUD.h"
#include "xrCore/AndroidCpuProfile.h"

#include "FBasicVisual.h"
#include "SkeletonCustom.h"
#include "FLOD.h"
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
#include "FVisual.h"
#include "Layers/xrRenderGL/glStaticIndexBatch.h"
#include "Layers/xrRenderGL/glSpotWriterAudit.h"
#endif
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "FTreeVisual.h"
#include "Layers/xrRenderGL/glGpuProfiler.h"
#endif

extern ENGINE_API float psHUD_FOV;

namespace xray::render::RENDER_NAMESPACE
{
using namespace R_dsgraph;

extern float r_ssaHZBvsTEX;
extern float r_ssaGLOD_start, r_ssaGLOD_end;

ICF float calcLOD(float ssa /*fDistSq*/, float /*R*/)
{
    return _sqrt(clampr((ssa - r_ssaGLOD_end) / (r_ssaGLOD_start - r_ssaGLOD_end), 0.f, 1.f));
}

template <class T>
bool cmp_ssa(const T &lhs, const T &rhs)
{
    return lhs.ssa > rhs.ssa;
}

// Sorting by SSA and changes minimizations
template <typename T>
bool cmp_pass(const T& left, const T& right)
{
    // std::sort requires a strict order: equal SSA compares false both ways.
    // Pass states are already interned and grouped by the SPass* map key.
    return left->second.ssa > right->second.ssa;
}

#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
static bool SameStripTreeMesh(const FTreeVisual_ST& first, const FTreeVisual_ST& next)
{
    return first.rm_geom == next.rm_geom && first.vBase == next.vBase && first.vCount == next.vCount &&
        first.iBase == next.iBase && first.dwPrimitives == next.dwPrimitives;
}
#endif

static bool BatchProgram(const SPass& pass, bool shadow)
{
    const char* program = pass.pp ? pass.pp->cName.c_str() : "";
    const char* pixel = strchr(program, '|');
    const char* geometry = strrchr(program, '|');
    if (!pixel || pixel == geometry || (geometry[1] && strcmp(geometry + 1, "null") != 0))
        return false;
    // These built-in deferred/shadow programs do not use vertex or primitive IDs.
    const char* prefix = shadow ? "shadow_" : "deffer_";
    return strncmp(program, prefix, 7) == 0 &&
        (strncmp(pixel + 1, prefix, 7) == 0 ||
            (shadow && strncmp(pixel + 1, "dumb|", 5) == 0));
}

template <typename DrawItem>
static void DrawStaticBatchPass(CBackend& backend, const mapNormalItems& items, bool shadow,
    GLStaticIndexBatch::Counters* counters, const DrawItem& drawItem)
{
    std::array<const IRender_Mesh*, 128> meshes;
    std::array<const _NormalItem*, 128> originalItems;
    SGeometry* geometry = nullptr;
    u32 count = 0, indices = 0, vertices = 0;
    const auto flush = [&]
    {
        if (!count)
            return;
        const GLuint buffer = count > 1 ? GLStaticIndexBatch::Buffer(meshes.data(), count, indices, counters) : 0;
        if (buffer)
        {
            backend.set_GeometryForIndexedDraw(geometry);
            backend.RenderStaticIndices(buffer, indices, vertices, count);
            backend.stat.r.s_static.add(vertices);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
            for (u32 i = 0; i < count; ++i)
                GLGpuProfiler::RecordVisual(MT_NORMAL);
#endif
        }
        else
            for (u32 i = 0; i < count; ++i)
                drawItem(*originalItems[i]);
        count = indices = vertices = 0;
    };

    for (const auto& item : items)
    {
        if (item.pVisual->Type == MT_NORMAL)
        {
            const auto& visual = *static_cast<const Fvisual*>(item.pVisual);
            const IRender_Mesh& mesh = visual.m_fast &&
                (ps_r1_force_geomx || (shadow && !backend.is_TessEnabled())) ?
                static_cast<const IRender_Mesh&>(*visual.m_fast) : visual;
            auto* next = mesh.rm_geom._get();
            if (next && next->vb && next->dcl && mesh.p_rm_Indices && mesh.dwPrimitives &&
                mesh.dwPrimitives <= GLStaticIndexBatch::MaxBatchIndices / 3 &&
                mesh.p_rm_Indices->ReadOnlyIndices(mesh.iBase, mesh.dwPrimitives * 3))
            {
                const u32 meshIndices = mesh.dwPrimitives * 3;
                if (count && (geometry->vb != next->vb || geometry->dcl != next->dcl ||
                    geometry->vb_stride != next->vb_stride || count == meshes.size() ||
                    meshIndices > GLStaticIndexBatch::MaxBatchIndices - indices || mesh.vCount > u32(-1) - vertices))
                    flush();
                geometry = next;
                meshes[count] = &mesh;
                originalItems[count++] = &item;
                indices += meshIndices;
                vertices += mesh.vCount;
                continue;
            }
        }
        flush();
        drawItem(item);
    }
    flush();
}

void R_dsgraph_structure::render_graph(u32 _priority, bool localShadowBatch)
#else
void R_dsgraph_structure::render_graph(u32 _priority)
#endif
{
    PIX_EVENT_CTX(cmd_list, dsgraph_render_graph);
    RImplementation.BasicStats.Primitives.Begin(); // XXX: Refactor a bit later

    cmd_list.set_xform_world(Fidentity);
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
    const bool shadow = o.phase == CRender::PHASE_SMAP;
    const bool staticStream = _priority == 0 &&
        ((localShadowBatch && shadow && GLStaticIndexBatch::ShadowEnabled()) ||
            (o.phase == CRender::PHASE_NORMAL && o.is_main_pass && GLStaticIndexBatch::MainEnabled())) &&
        !GLSpotWriterAudit::Enabled() && GLStaticIndexBatch::Begin();
    auto* const batchCounters = staticStream ? GLStaticIndexBatch::SampleCounters(shadow) : nullptr;
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    bool probeDepthPrefix = _priority == 0 && localShadowBatch && shadow &&
        Device.dwFrame % 67 == 0 &&
        AndroidSceneCompare::HasOption(Core.Params, "-gles_shadow_depth_prefix_probe");
    u32 opaquePrefixDraws = 0;
#endif
#endif

#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
    auto* sortTiming = AndroidCpuProfile::SampleInside(AndroidCpuProfile::Phase::LocalSubmit,
        AndroidCpuProfile::Phase::LocalSort);
    const auto sortItems = [sortTiming](auto& items, auto compare)
#else
    const auto sortItems = [](auto& items, auto compare)
#endif
    {
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
        AndroidCpuProfile::Timer probe(items.size() > 1 ? sortTiming : nullptr);
#endif
        std::sort(items.begin(), items.end(), compare);
    };

    // **************************************************** NORMAL
    // Perform sorting based on ScreenSpaceArea
    // Sorting by SSA and changes minimizations
    // Render several passes
    {
        ZoneScopedN("dsgraph_render_static");
        PIX_EVENT_CTX(cmd_list, dsgraph_render_static);
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope mainStatic(GLGpuProfiler::Stage::MainStatic, false,
            GLGpuProfiler::InStage(GLGpuProfiler::Stage::MainGeometry));
#endif

        for (u32 iPass = 0; iPass < SHADER_PASSES_MAX; ++iPass)
        {
            auto& map = mapNormalPasses[_priority][iPass];

            map.get_any_p(nrmPasses);
            sortItems(nrmPasses, cmp_pass<mapNormal_T::value_type*>);
            for (const auto& it : nrmPasses)
            {
                cmd_list.set_Pass(it->first);
                cmd_list.apply_lmaterial();

                mapNormalItems& items = it->second;
                items.ssa = 0;

                sortItems(items, cmp_ssa<_NormalItem>);
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                if (!staticStream && GLGpuProfiler::InStage(GLGpuProfiler::Stage::MainStatic))
                {
                    const FTreeVisual_ST* previous = nullptr;
                    unsigned trees = 0, adjacentMatches = 0;
                    for (const auto& item : items)
                    {
                        if (item.pVisual->Type != MT_TREE_ST)
                        {
                            previous = nullptr;
                            continue;
                        }
                        const auto* tree = static_cast<const FTreeVisual_ST*>(item.pVisual);
                        ++trees;
                        adjacentMatches += previous && SameStripTreeMesh(*previous, *tree);
                        previous = tree;
                    }
                    GLGpuProfiler::RecordStripTrees(trees, adjacentMatches);
                }
#endif
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                if (probeDepthPrefix)
                {
                    const char* program = it->first->pp ? it->first->pp->cName.c_str() : "";
                    if (strcmp(program, "shadow_direct_base|dumb|null") == 0 ||
                        strcmp(program, "shadow_direct_base_0|dumb|null") == 0)
                    {
                        for (const auto& item : items)
                        {
                            if (item.pVisual->Type != MT_NORMAL)
                            {
                                probeDepthPrefix = false;
                                break;
                            }
                            ++opaquePrefixDraws;
                        }
                    }
                    else
                        probeDepthPrefix = false;
                }
#endif
                const auto drawItem = [&](const _NormalItem& item)
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    GLGpuProfiler::RecordVisual(item.pVisual->Type);
#endif
                    const float LOD = calcLOD(item.ssa, item.pVisual->vis.sphere.R);
#ifdef USE_DX11
                    cmd_list.LOD.set_LOD(LOD);
#endif
                    item.pVisual->Render(cmd_list, LOD, o.phase == CRender::PHASE_SMAP);
                };
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
                if (staticStream && items.size() > 1 && BatchProgram(*it->first, shadow))
                    DrawStaticBatchPass(cmd_list, items, shadow, batchCounters, drawItem);
                else
#endif
                    for (const auto& item : items)
                        drawItem(item);
                items.clear();

            }
            nrmPasses.clear();
            map.clear();
        }
    }

    // **************************************************** MATRIX
    // Perform sorting based on ScreenSpaceArea
    // Sorting by SSA and changes minimizations
    // Render several passes
    {
        ZoneScopedN("dsgraph_render_dynamic");
        PIX_EVENT_CTX(cmd_list, dsgraph_render_dynamic);
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope mainDynamic(GLGpuProfiler::Stage::MainDynamic, false,
            GLGpuProfiler::InStage(GLGpuProfiler::Stage::MainGeometry));
#endif

        for (u32 iPass = 0; iPass < SHADER_PASSES_MAX; ++iPass)
        {
            auto& map = mapMatrixPasses[_priority][iPass];

            map.get_any_p(matPasses);
            sortItems(matPasses, cmp_pass<mapMatrix_T::value_type*>);
            for (const auto& it : matPasses)
            {
                cmd_list.set_Pass(it->first);

                mapMatrixItems& items = it->second;
                items.ssa = 0;

                sortItems(items, cmp_ssa<_MatrixItem>);
                for (auto& item : items)
                {
                    cmd_list.set_xform_world(item.Matrix);
                    RImplementation.apply_object(cmd_list, item.pObject);
                    cmd_list.apply_lmaterial();

                    const float LOD = calcLOD(item.ssa, item.pVisual->vis.sphere.R);
#ifdef USE_DX11
                    cmd_list.LOD.set_LOD(LOD);
#endif
                    // --#SM+#-- Обновляем шейдерные данные модели [update shader values for this model]
                    // RCache.hemi.c_update(item.pVisual);

#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    GLGpuProfiler::RecordVisual(item.pVisual->Type);
#endif
                    item.pVisual->Render(cmd_list, LOD, o.phase == CRender::PHASE_SMAP);
                }
                items.clear();
            }
            matPasses.clear();
            map.clear();
        }
    }

    RImplementation.BasicStats.Primitives.End(); // XXX: Refactor a bit later
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    if (opaquePrefixDraws)
        Msg("* [gles_shadow_depth_prefix] frame=%u opaque_draws=%u", Device.dwFrame, opaquePrefixDraws);
#endif
}

//////////////////////////////////////////////////////////////////////////
// Helper classes and functions

/*
Предназначен для установки режима отрисовки HUD и возврата оригинального после отрисовки.
*/
class hud_transform_helper
{
    Fmatrix Pold;
    static u32 cullMode;
    static bool isActive;

    CBackend& cmd_list;

public:
    explicit hud_transform_helper(CBackend& cmd_list_in)
        : cmd_list(cmd_list_in)
    {
        // Change projection
        Pold  = Device.mProject;

        // XXX: Xottab_DUTY: custom FOV. Implement it someday
        // It should be something like this:
        // float customFOV;
        // if (isCustomFOV)
        //     customFOV = V->getVisData().obj_data->m_hud_custom_fov;
        // else
        //     customFOV = psHUD_FOV * Device.fFOV;
        //
        // Device.mProject.build_projection(deg2rad(customFOV), Device.fASPECT,
        //    VIEWPORT_NEAR, g_pGamePersistent->Environment().CurrentEnv.far_plane);
        //
        // Look at the function:
        // void __fastcall sorted_L1_HUD(mapSorted_Node* N)
        // In the commit:
        // https://github.com/ShokerStlk/xray-16-SWM/commit/869de0b6e74ac05990f541e006894b6fe78bd2a5#diff-4199ef700b18ce4da0e2b45dee1924d0R83

        Fmatrix prj_new;
        prj_new.build_projection(deg2rad(psHUD_FOV * Device.fFOV /* *Device.fASPECT*/), Device.fASPECT,
            HUD_VIEWPORT_NEAR, g_pGamePersistent->Environment().CurrentEnv.far_plane);
        cmd_list.set_xform_project(prj_new);

        RImplementation.rmNear(cmd_list);

        // preserve culling mode
        cullMode = cmd_list.get_CullMode();
        isActive = true;
    }

    ~hud_transform_helper()
    {
        RImplementation.rmNormal(cmd_list);

        // Restore projection
        cmd_list.set_xform_project(Pold);
        // restore culling mode
        cmd_list.set_CullMode(cullMode);
        isActive = false;
    }

    static void apply_custom_state(CBackend& cmd_list)
    {
        if (!isActive || !psHUD_Flags.test(HUD_LEFT_HANDED))
            return;

        // Change culling mode if HUD meshes were flipped
        if (cullMode != CULL_NONE)
        {
            cmd_list.set_CullMode(cullMode == CULL_CW ? CULL_CCW : CULL_CW);
        }
    }
};

u32 hud_transform_helper::cullMode = CULL_NONE;
bool hud_transform_helper::isActive = false;

template<class T>
void __fastcall render_item(u32 context_id, const T& item)
{
    auto& dsgraph = RImplementation.get_context(context_id);

    dxRender_Visual* V = item.second.pVisual;
    VERIFY(V && V->shader._get());
    dsgraph.cmd_list.set_Element(item.second.se);
    dsgraph.cmd_list.set_xform_world(item.second.Matrix);
    RImplementation.apply_object(dsgraph.cmd_list, item.second.pObject);
    dsgraph.cmd_list.apply_lmaterial();
    hud_transform_helper::apply_custom_state(dsgraph.cmd_list);
    //--#SM+#-- Обновляем шейдерные данные модели [update shader values for this model]
    //RCache.hemi.c_update(V);
    V->Render(dsgraph.cmd_list, calcLOD(item.first, V->vis.sphere.R), dsgraph.o.phase == CRender::PHASE_SMAP);
}

template<class T>
ICF void sort_front_to_back_render_and_clean(u32 context_id, T& vec)
{
    vec.traverse_left_right(context_id, render_item);
    vec.clear();
}

template<class T>
ICF void sort_back_to_front_render_and_clean(u32 context_id, T& vec)
{
    vec.traverse_right_left(context_id, render_item);
    vec.clear();
}

//////////////////////////////////////////////////////////////////////////
// HUD render
void R_dsgraph_structure::render_hud()
{
    ZoneScoped;
    PIX_EVENT_CTX(cmd_list, dsgraph_render_hud);
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope mainHud(GLGpuProfiler::Stage::MainHud, false,
        GLGpuProfiler::InStage(GLGpuProfiler::Stage::MainGeometry));
#endif

    if (!mapHUD.empty())
    {
        hud_transform_helper helper{ cmd_list };
        sort_front_to_back_render_and_clean(context_id, mapHUD);
    }

#if RENDER == R_R1
    if (g_pGameLevel->pHUD && g_pGameLevel->pHUD->RenderActiveItemUIQuery())
        render_hud_ui(); // hud ui
#endif
}

void R_dsgraph_structure::render_hud_ui()
{
    ZoneScoped;
    CCustomHUD* levelHud = g_pGameLevel->pHUD;
    VERIFY(levelHud && levelHud->RenderActiveItemUIQuery());

    PIX_EVENT_CTX(cmd_list, dsgraph_render_hud_ui);

    hud_transform_helper helper{ cmd_list };

#if RENDER != R_R1
    // Targets, use accumulator for temporary storage
    const ref_rt rt_null;
    cmd_list.set_RT(0, 1);
    cmd_list.set_RT(0, 2);
    auto zb = RImplementation.Target->rt_Base_Depth;

#if (RENDER == R_R3) || (RENDER == R_R4) || (RENDER==R_GL)
    if (RImplementation.o.msaa)
        zb = RImplementation.Target->rt_MSAADepth;
#endif

    RImplementation.Target->u_setrt(cmd_list,
        RImplementation.o.albedo_wo ? RImplementation.Target->rt_Accumulator : RImplementation.Target->rt_Color,
        rt_null, rt_null, zb);
#endif // RENDER!=R_R1

    levelHud->RenderActiveItemUI();
}

//////////////////////////////////////////////////////////////////////////
// strict-sorted render
void R_dsgraph_structure::render_sorted()
{
    ZoneScoped;
    PIX_EVENT_CTX(cmd_list, dsgraph_render_sorted);

    sort_back_to_front_render_and_clean(context_id, mapSorted);

    if (!mapHUDSorted.empty())
    {
        hud_transform_helper helper{ cmd_list };
        sort_back_to_front_render_and_clean(context_id, mapHUDSorted);
    }
}

//////////////////////////////////////////////////////////////////////////
// strict-sorted render
void R_dsgraph_structure::render_emissive()
{
#if RENDER != R_R1
    ZoneScoped;
    PIX_EVENT_CTX(cmd_list, dsgraph_render_emissive);

    sort_front_to_back_render_and_clean(context_id, mapEmissive);

    if (!mapHUDEmissive.empty())
    {
        hud_transform_helper helper{ cmd_list };
        sort_front_to_back_render_and_clean(context_id, mapHUDEmissive);
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
// strict-sorted render
void R_dsgraph_structure::render_wmarks()
{
#if RENDER != R_R1
    ZoneScoped;
    PIX_EVENT(dsgraph_render_wmarks);

    sort_front_to_back_render_and_clean(context_id, mapWmark);
#endif
}

//////////////////////////////////////////////////////////////////////////
// strict-sorted render
void R_dsgraph_structure::render_distort()
{
    ZoneScoped;
    PIX_EVENT(dsgraph_render_distort);

    sort_back_to_front_render_and_clean(context_id, mapDistort);
}

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
void R_dsgraph_structure::render_olr_r1_lightmaps()
{
    if (mapOlrR1Lightmaps.empty())
        return;

    static const bool logged = [] {
        Msg("* OLR R1 lightmaps: drawing default static materials in the forward pass");
        return true;
    }();
    (void)logged;
    sort_front_to_back_render_and_clean(context_id, mapOlrR1Lightmaps);
}
#endif

void R_dsgraph_structure::render_R1_box(IRender_Sector::sector_id_t sector_id, Fbox& BB, int sh)
{
    VERIFY(sector_id != IRender_Sector::INVALID_SECTOR_ID);
    auto* S = Sectors[sector_id];

    PIX_EVENT(dsgraph_render_R1_box);

    lstVisuals.clear();
    lstVisuals.push_back(((CSector*)S)->root());

    for (size_t test = 0; test < lstVisuals.size(); ++test)
    {
        dxRender_Visual* V = lstVisuals[test];

        // Visual is 100% visible - simply add it
        switch (V->Type)
        {
        case MT_HIERRARHY:
        {
            // Add all children
            FHierrarhyVisual* pV = (FHierrarhyVisual*)V;
            for (auto& i : pV->children)
            {
                dxRender_Visual* T = i;
                if (BB.intersect(T->vis.box))
                    lstVisuals.push_back(T);
            }
        }
        break;
        case MT_SKELETON_ANIM:
        case MT_SKELETON_RIGID:
        {
            // Add all children	(s)
            CKinematics* pV = (CKinematics*)V;
            pV->CalculateBones(TRUE);
            for (auto& i : pV->children)
            {
                dxRender_Visual* T = i;
                if (BB.intersect(T->vis.box))
                    lstVisuals.push_back(T);
            }
        }
        break;
        case MT_LOD:
        {
            FLOD* pV = (FLOD*)V;
            for (auto& i : pV->children)
            {
                dxRender_Visual* T = i;
                if (BB.intersect(T->vis.box))
                    lstVisuals.push_back(T);
            }
        }
        break;
        default:
        {
            // Renderable visual
            ShaderElement* E2 = V->shader->E[sh]._get();
            if (E2 && !(E2->flags.bDistort))
            {
                for (u32 pass = 0; pass < E2->passes.size(); pass++)
                {
                    cmd_list.set_Element(E2, pass);
                    V->Render(cmd_list, -1.f, o.phase == CRender::PHASE_SMAP);
                }
            }
        }
        break;
        }
    }
}
} // namespace xray::render::RENDER_NAMESPACE
