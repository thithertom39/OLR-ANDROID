#include "stdafx.h"

#include "xrEngine/IRenderable.h"
#include "xrEngine/CustomHUD.h"
#include "xrCore/AndroidCpuProfile.h"

#include "FBasicVisual.h"
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "Layers/xrRenderGL/glGpuProfiler.h"
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
#include "light.h"
#include "xrRender_console.h"
#include "olr_r1_mode.h"
#include <cmath>
#endif
#include "SkeletonCustom.h"
#include "FLOD.h"
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
#include "FHierrarhyVisual.h"
#endif

extern ENGINE_API float psHUD_FOV;

namespace xray::render::RENDER_NAMESPACE
{
using namespace R_dsgraph;

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
// Per-visual local light constants are written only during the optional
// static forward pass. Unlike the old camera-global selection, this uses
// each static visual's world-space bounds to avoid proximity popping.
static thread_local bool g_olrR1ForwardVisualPass = false;
static bool olr_r1_multipass_enabled()
{
    return Core.Params && strstr(Core.Params, "-olr_r1_forward") &&
        strstr(Core.Params, "-olr_r1_multipass");
}
void olr_r1_forward_bind_visual_lights(CBackend& cmd_list, const Fsphere& bounds);

static thread_local R_dsgraph_structure* g_olrR1LightContext = nullptr;
static thread_local xr_map<const light*, xr_set<dxRender_Visual*>> g_olrR1LightVisuals;
static bool olr_r1_cached_submission()
{
    static const bool enabled = olr_r1_native_enabled() && !strstr(Core.Params, "-olr_r1_light_cache_off");
    return enabled;
}

struct OlrR1PreparedLight
{
    const light* source;
    Fvector4 position, direction, color;
    Fmatrix spotTransform;
};
static thread_local xr_vector<OlrR1PreparedLight> g_olrR1PreparedLights;

static void olr_r1_light_camera(const light& source, Fmatrix& combined, Fvector& origin)
{
    Fvector direction, up;
    Fmatrix view, projection;
    origin = source.position;
    if (source.flags.type == IRender_Light::SPOT)
    {
        direction = source.direction;
        direction.normalize();
        up.set(0.f, 1.f, 0.f);
        if (_abs(up.dotproduct(direction)) > .99f)
            up.set(0.f, 0.f, 1.f);
        projection.build_projection(source.cone, 1.f, .1f, source.range + EPS_S);
    }
    else
    {
        // LightPPA::render_point's original top-down visibility volume.
        origin.y += 300.f;
        direction.set(0.f, -1.f, 0.f);
        up.set(0.f, 0.f, 1.f);
        projection.build_projection(deg2rad(2.f), 1.f, 300.f - source.range, 300.f + source.range);
    }
    view.build_camera_dir(origin, direction, up);
    combined.mul(projection, view);
}

static void olr_r1_prepare_lights()
{
    g_olrR1PreparedLights.clear();
    // These values are invariant during this forward pass. Keep package order:
    // RGBA8 rounds/saturates after EACH draw, so do not regroup light passes.
    const auto append = [](const light* entry)
    {
        const light* source = entry && entry->shadow_parent ? entry->shadow_parent : entry;
        if (!source || !source->flags.bActive || source->flags.bStatic || source->range <= EPS_L ||
            (source->flags.type != IRender_Light::POINT && source->flags.type != IRender_Light::SPOT))
            return;
        for (const auto& prepared : g_olrR1PreparedLights)
            if (prepared.source == source)
                return;
        const float distance = Device.vCameraPosition.distance_to(source->spatial.sphere.P) - source->spatial.sphere.R;
        const float scale = clampr(1.f - distance / _max(ps_r1_dlights_clip, EPS_L), 0.f, 1.f);
        if (scale <= EPS_L)
            return;
        OlrR1PreparedLight prepared;
        prepared.spotTransform.identity();
        prepared.source = source;
        prepared.position.set(source->position.x, source->position.y, source->position.z, .5f / source->range);
        prepared.color.set(source->color.r * scale, source->color.g * scale, source->color.b * scale, 1.f);
        prepared.direction.set(0.f, 0.f, 0.f, -2.f);
        if (source->flags.type == IRender_Light::SPOT)
        {
            Fvector dir = source->direction;
            if (dir.square_magnitude() <= .0001f)
                return;
            dir.normalize();
            prepared.direction.set(dir.x, dir.y, dir.z, std::cos(source->cone * .5f));
            Fmatrix combined;
            Fvector origin;
            olr_r1_light_camera(*source, combined, origin);
            const Fmatrix adjust = {
                .5f, 0.f, 0.f, 0.f,
                0.f, -.5f, 0.f, 0.f,
                0.f, 0.f, 1.f / source->range, 0.f,
                .5f + .5f / 32.f, .5f + .5f / 32.f, 0.f, 1.f};
            prepared.spotTransform.mul(adjust, combined);
        }
        g_olrR1PreparedLights.push_back(prepared);
    };
    for (const light* source : RImplementation.Lights.package.v_point) append(source);
    for (const light* source : RImplementation.Lights.package.v_spot) append(source);
    for (const light* source : RImplementation.Lights.package.v_shadowed) append(source);
}

static void olr_r1_collect_light_visuals(dxRender_Visual* visual, const CFrustum& frustum,
    u32 mask, xr_set<dxRender_Visual*>& visible)
{
    auto& bounds = visual->vis;
    if (frustum.testSAABB(bounds.sphere.P, bounds.sphere.R, bounds.box.data(), mask) == fcvNone)
        return;
    switch (visual->Type)
    {
    case MT_HIERRARHY:
    case MT_LOD:
        for (auto* child : static_cast<FHierrarhyVisual*>(visual)->children)
            olr_r1_collect_light_visuals(child, frustum, mask, visible);
        break;
    case MT_SKELETON_ANIM:
    case MT_SKELETON_RIGID:
        for (auto* child : static_cast<CKinematics*>(visual)->children)
            olr_r1_collect_light_visuals(child, frustum, mask, visible);
        break;
    default:
        visible.insert(visual);
        break;
    }
}

static bool olr_r1_light_reaches_visual(const light& source, dxRender_Visual* visual)
{
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope traversalScope(GLGpuProfiler::Stage::R1Traversal, false);
#endif
    if (!g_olrR1LightContext)
        return false;
    auto& graph = *g_olrR1LightContext;
    auto found = g_olrR1LightVisuals.find(&source);
    if (found == g_olrR1LightVisuals.end())
    {
        auto& visible = g_olrR1LightVisuals[&source];
        auto sector = source.spatial.sector_id;
        if (sector == IRender_Sector::INVALID_SECTOR_ID)
            sector = graph.detect_sector(source.position);
        if (sector == IRender_Sector::INVALID_SECTOR_ID || sector >= graph.Sectors.size())
            return false;
        Fmatrix combined;
        Fvector origin;
        olr_r1_light_camera(source, combined, origin);
        CFrustum frustum;
        frustum.CreateFromMatrix(combined, FRUSTUM_P_ALL);
        // These sectors/portals belong to a separately allocated CPU context.
        // Never overwrite the camera traversal or another shadow worker's lists.
        for (auto* portal : graph.Portals)
            portal->bDualRender = FALSE;
        if (source.flags.type == IRender_Light::SPOT && RImplementation.rmPortals)
        {
            Fvector radius;
            radius.set(EPS_L * 20.f, EPS_L * 20.f, EPS_L * 20.f);
            graph.Sectors_xrc.box_query(CDB::OPT_FULL_TEST, RImplementation.rmPortals, origin, radius);
            for (size_t i = 0; i < graph.Sectors_xrc.r_count(); ++i)
            {
                const auto portalId = RImplementation.rmPortals->get_tris()[graph.Sectors_xrc.r_begin()[i].id].dummy;
                graph.Portals[portalId]->bDualRender = TRUE;
            }
        }
        graph.PortalTraverser.traverse(graph.Sectors[sector], frustum, origin, combined, 0);
        for (auto* reached : graph.PortalTraverser.r_sectors)
            for (const auto& clipped : reached->r_frustums)
                olr_r1_collect_light_visuals(reached->root(), clipped, clipped.getMask(), visible);
        found = g_olrR1LightVisuals.find(&source);
    }
    return found->second.find(visual) != found->second.end();
}
#endif

extern float r_ssaHZBvsTEX;
extern float r_ssaGLOD_start, r_ssaGLOD_end;

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
void R_dsgraph_structure::record_olr_r1_receiver(dxRender_Visual* visual, const Fmatrix& world)
{
    if (!olr_r1_scene_enabled() || !o.is_main_pass || o.phase != CRender::PHASE_NORMAL)
        return;
    Fbox bounds;
    bounds.xform(visual->vis.box, world);
    Fsphere sphere;
    bounds.getsphere(sphere.P, sphere.R);
    if (!bounds.is_valid() || !std::isfinite(sphere.R) || !std::isfinite(sphere.P.x) ||
        !std::isfinite(sphere.P.y) || !std::isfinite(sphere.P.z))
        olr_r1_receiver_bounds_valid = false;
    else
        olr_r1_deferred_receivers.push_back(sphere);
}
#endif

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

void R_dsgraph_structure::render_graph(u32 _priority)
{
    PIX_EVENT_CTX(cmd_list, dsgraph_render_graph);
    RImplementation.BasicStats.Primitives.Begin(); // XXX: Refactor a bit later

    cmd_list.set_xform_world(Fidentity);

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
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                const u32 beforeDraws = cmd_list.stat.render.calls;
#endif
                items.ssa = 0;

                sortItems(items, cmp_ssa<_NormalItem>);
                for (const auto& item : items)
                {
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                    if (_priority == 0) record_olr_r1_receiver(item.pVisual, Fidentity);
#endif
                    const float LOD = calcLOD(item.ssa, item.pVisual->vis.sphere.R);
#ifdef USE_DX11
                    cmd_list.LOD.set_LOD(LOD);
#endif
                    // --#SM+#-- Обновляем шейдерные данные модели [update shader values for this model]
                    // RCache.hemi.c_update(item.pVisual);

                    item.pVisual->Render(cmd_list, LOD, o.phase == CRender::PHASE_SMAP);
                }
                items.clear();
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                if (o.is_main_pass && o.phase == CRender::PHASE_NORMAL)
                    GLGpuProfiler::RecordR1Material(GLGpuProfiler::MaterialName(it->first),cmd_list.stat.render.calls-beforeDraws,_priority == 0 ? 4 : 0);
#endif

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

        for (u32 iPass = 0; iPass < SHADER_PASSES_MAX; ++iPass)
        {
            auto& map = mapMatrixPasses[_priority][iPass];

            map.get_any_p(matPasses);
            sortItems(matPasses, cmp_pass<mapMatrix_T::value_type*>);
            for (const auto& it : matPasses)
            {
                cmd_list.set_Pass(it->first);

                mapMatrixItems& items = it->second;
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                const u32 beforeDraws = cmd_list.stat.render.calls;
#endif
                items.ssa = 0;

                sortItems(items, cmp_ssa<_MatrixItem>);
                for (auto& item : items)
                {
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                    if (_priority == 0) record_olr_r1_receiver(item.pVisual, item.Matrix);
#endif
                    cmd_list.set_xform_world(item.Matrix);
                    RImplementation.apply_object(cmd_list, item.pObject);
                    cmd_list.apply_lmaterial();

                    const float LOD = calcLOD(item.ssa, item.pVisual->vis.sphere.R);
#ifdef USE_DX11
                    cmd_list.LOD.set_LOD(LOD);
#endif
                    // --#SM+#-- Обновляем шейдерные данные модели [update shader values for this model]
                    // RCache.hemi.c_update(item.pVisual);

                    item.pVisual->Render(cmd_list, LOD, o.phase == CRender::PHASE_SMAP);
                }
                items.clear();
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                if (o.is_main_pass && o.phase == CRender::PHASE_NORMAL)
                    GLGpuProfiler::RecordR1Material(GLGpuProfiler::MaterialName(it->first),cmd_list.stat.render.calls-beforeDraws,_priority == 0 ? 4 : 0);
#endif
            }
            matPasses.clear();
            map.clear();
        }
    }

    RImplementation.BasicStats.Primitives.End(); // XXX: Refactor a bit later
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
    static bool active() { return isActive; }
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
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    if (!g_olrR1ForwardVisualPass && hud_transform_helper::active())
        dsgraph.record_olr_r1_receiver(V, item.second.Matrix);
#endif
    dsgraph.cmd_list.set_Element(item.second.se);
    // Native static shaders use authored/environment lighting, not R2 ROS
    // material constants. Tree visuals may change W in Render(), so compare
    // the actual backend transform rather than remembering the previous item.
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    const bool cachedStatic = g_olrR1ForwardVisualPass && olr_r1_cached_submission() && !item.second.pObject;
    if (!cachedStatic || memcmp(&dsgraph.cmd_list.get_xform_world(), &item.second.Matrix, sizeof(Fmatrix)) != 0)
#endif
        dsgraph.cmd_list.set_xform_world(item.second.Matrix);
    RImplementation.apply_object(dsgraph.cmd_list, item.second.pObject);
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    if (!cachedStatic)
#endif
        dsgraph.cmd_list.apply_lmaterial();
    hud_transform_helper::apply_custom_state(dsgraph.cmd_list);
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    if (g_olrR1ForwardVisualPass && V->shader->olr_r1_alpha_ref >= 0)
        dsgraph.cmd_list.set_c("olr_r1_alpha_ref", float(V->shader->olr_r1_alpha_ref), 0.f, 0.f, 0.f);
    // Must run after set_Element/apply_lmaterial: CBackend may cache the
    // previous shader and does not automatically rebind local light data.
    if (g_olrR1ForwardVisualPass && !olr_r1_multipass_enabled())
        olr_r1_forward_bind_visual_lights(dsgraph.cmd_list, V->vis.sphere);
#endif
    //--#SM+#-- Обновляем шейдерные данные модели [update shader values for this model]
    //RCache.hemi.c_update(V);
    const float visualLOD = calcLOD(item.first, V->vis.sphere.R);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    const u32 beforeDraws = dsgraph.cmd_list.stat.render.calls;
#endif
    V->Render(dsgraph.cmd_list, visualLOD, dsgraph.o.phase == CRender::PHASE_SMAP);
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    if (dsgraph.o.is_main_pass && dsgraph.o.phase == CRender::PHASE_NORMAL && !item.second.se->passes.empty())
        GLGpuProfiler::RecordR1Material(GLGpuProfiler::MaterialName(item.second.se->passes[0]._get()),
            dsgraph.cmd_list.stat.render.calls-beforeDraws,g_olrR1ForwardVisualPass ? 1 : hud_transform_helper::active() ? 4 : 0);
#endif

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    // Optional R1-like dynamic light traversal. One baked world draw above,
    // then one additive draw for EACH intersecting non-baked POINT/SPOT light,
    // including the player torch. No arbitrary three-light shader slot cap.
    // The pass is built only for supported opaque Lua lightmap materials.
    if (g_olrR1ForwardVisualPass && olr_r1_multipass_enabled())
    {
        ShaderElement* extra = V->shader->olr_r1_lightmap_dynamic._get();
        if (!extra)
            return;

        static thread_local xr_vector<const light*> visited;
        visited.clear();
        const auto drawLight = [&](const light* entry, const OlrR1PreparedLight* prepared)
        {
            const light* source = prepared ? prepared->source : (entry && entry->shadow_parent ? entry->shadow_parent : entry);
            if (!prepared && (!source || !source->flags.bActive || source->flags.bStatic ||
                source->range <= EPS_L ||
                (source->flags.type != IRender_Light::POINT &&
                 source->flags.type != IRender_Light::SPOT)))
                return;

            if (!prepared)
            {
                for (const light* alreadyDrawn : visited)
                    if (alreadyDrawn == source)
                        return;
                visited.push_back(source);
            }

            const float maxReach = source->range + V->vis.sphere.R;
            if (source->position.distance_to_sqr(V->vis.sphere.P) > maxReach * maxReach)
                return;

            // Match the original R1 adjustable distant-light fade.
            // r1_dlights_clip is not a hard cutoff at the light's own radius:
            // the original R1 fades contribution by camera-to-light volume
            // distance and lets users increase this budget.
            const bool native = olr_r1_native_enabled();
            const float clipDistance = prepared ? 0.f : native
                ? Device.vCameraPosition.distance_to(source->spatial.sphere.P) - source->spatial.sphere.R
                : Device.vCameraPosition.distance_to(source->position) - source->range;
            const float clipRange = _max(ps_r1_dlights_clip, EPS_L);
            const float lightScale = prepared ? 1.f : clampr(1.f - clipDistance / clipRange, 0.f, 1.f);
            if (lightScale <= EPS_L)
                return;
            if (native && !olr_r1_light_reaches_visual(*source, V))
                return;

            Fvector4 pos, direction, color;
            if (prepared)
            {
                pos = prepared->position;
                direction = prepared->direction;
                color = prepared->color;
            }
            else
            {
            pos.set(source->position.x, source->position.y, source->position.z,
                1.f / (source->range * source->range));
            color.set(source->color.r * lightScale, source->color.g * lightScale,
                source->color.b * lightScale, 1.f);
            direction.set(0.f, 0.f, 0.f, -2.f); // point, no cone
            if (source->flags.type == IRender_Light::SPOT)
            {
                Fvector dir = source->direction;
                const float dirSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
                if (dirSq <= 0.0001f)
                    return;
                dir.normalize();
                direction.set(dir.x, dir.y, dir.z, std::cos(source->cone * 0.5f));
            }
            }

            if (native && source->flags.type == IRender_Light::SPOT)
                extra = V->shader->olr_r1_lightmap_spot._get();
            else
                extra = V->shader->olr_r1_lightmap_dynamic._get();
            if (!extra)
                return;
            dsgraph.cmd_list.set_Element(extra);
            if (V->shader->olr_r1_alpha_ref >= 0)
                dsgraph.cmd_list.set_c("olr_r1_alpha_ref", float(V->shader->olr_r1_alpha_ref), 0.f, 0.f, 0.f);
            if (!cachedStatic)
                dsgraph.cmd_list.apply_lmaterial();
            if (native)
            {
                // LightPPA::cl_light_PR and render_spot, including its
                // authored 32-texel half-texel convention (not DDS width).
                pos.w = .5f / source->range;
                if (source->flags.type == IRender_Light::SPOT)
                {
                    if (prepared)
                        dsgraph.cmd_list.set_c("olr_r1_light_xform", prepared->spotTransform);
                    else
                    {
                    Fmatrix combined, xform;
                    Fvector origin;
                    olr_r1_light_camera(*source, combined, origin);
                    const Fmatrix adjust = {
                        .5f, 0.f, 0.f, 0.f,
                        0.f, -.5f, 0.f, 0.f,
                        0.f, 0.f, 1.f / source->range, 0.f,
                        .5f + .5f / 32.f, .5f + .5f / 32.f, 0.f, 1.f};
                    xform.mul(adjust, combined);
                    dsgraph.cmd_list.set_c("olr_r1_light_xform", xform);
                    }
                }
            }
            dsgraph.cmd_list.set_c("olr_r1_dynamic_pos", pos);
            dsgraph.cmd_list.set_c("olr_r1_dynamic_dir", direction);
            dsgraph.cmd_list.set_c("olr_r1_dynamic_color", color);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
            const u32 beforeLightDraws = dsgraph.cmd_list.stat.render.calls;
#endif
            V->Render(dsgraph.cmd_list, visualLOD, false);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
            GLGpuProfiler::RecordR1Material(GLGpuProfiler::MaterialName(extra->passes[0]._get()),
                dsgraph.cmd_list.stat.render.calls-beforeLightDraws,source->flags.type == IRender_Light::SPOT ? 3 : 2);
#endif
        };

        if (olr_r1_cached_submission())
            for (const auto& prepared : g_olrR1PreparedLights)
                drawLight(prepared.source, &prepared);
        else
        {
            for (const light* source : RImplementation.Lights.package.v_point) drawLight(source, nullptr);
            for (const light* source : RImplementation.Lights.package.v_spot) drawLight(source, nullptr);
            for (const light* source : RImplementation.Lights.package.v_shadowed) drawLight(source, nullptr);
        }
    }
#endif
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
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope worldScope(GLGpuProfiler::Stage::R1World);
#endif
    if (mapOlrR1Lightmaps.empty())
        return;

    static const bool logged = [] {
        if (olr_r1_native_enabled())
            Msg("* OLR R1 native: lmap/vertex base, original POINT/SPOT lookup, portal subspace, RGBA8 ONE+ONE; models/special materials remain deferred");
        else if (strstr(Core.Params, "-olr_r1_forward"))
            Msg("* OLR R1 forward: rendering eligible static lightmapped geometry directly; dynamic geometry remains deferred");
        else
            Msg("* OLR R1 lightmaps: add baked light via %s; R2 dynamic lights preserved",
                RImplementation.o.fp16_blend ? "HDR accumulator" : "LDR forward fallback");
        return true;
    }();
    (void)logged;
    const bool previous = g_olrR1ForwardVisualPass;
    g_olrR1ForwardVisualPass = Core.Params && strstr(Core.Params, "-olr_r1_forward");
    u32 lightContextId = R_dsgraph_structure::INVALID_CONTEXT_ID;
    if (olr_r1_native_enabled())
    {
        if (olr_r1_cached_submission())
            olr_r1_prepare_lights();
        g_olrR1LightVisuals.clear(); // moving/switched lights need a fresh subspace every frame.
        lightContextId = RImplementation.alloc_context(false);
        if (lightContextId != R_dsgraph_structure::INVALID_CONTEXT_ID)
            g_olrR1LightContext = &RImplementation.get_context(lightContextId);
        else
            Msg("! OLR R1 native: no CPU context for light subspace traversal");
    }
    sort_front_to_back_render_and_clean(context_id, mapOlrR1Lightmaps);
    if (lightContextId != R_dsgraph_structure::INVALID_CONTEXT_ID)
        RImplementation.release_context(lightContextId);
    g_olrR1LightContext = nullptr;
    g_olrR1LightVisuals.clear();
    g_olrR1ForwardVisualPass = previous;
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
