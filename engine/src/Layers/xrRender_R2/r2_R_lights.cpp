#include "stdafx.h"
#include "xrCore/AndroidCpuProfile.h"
#if defined(XR_PLATFORM_ANDROID)
#include <array>
#include <atomic>
#endif
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
#include "Layers/xrRenderGL/glDetailShadowCull.h"
#endif
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "Layers/xrRenderGL/glGpuProfiler.h"
#endif

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
#include "Layers/xrRenderGL/glSpotWriterAudit.h"
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
#include "Layers/xrRender/FBasicVisual.h"
#include "Layers/xrRender/LocalShadowStaticCache.h"
#include <optional>
#include <string>
#include <unordered_map>
#endif

namespace xray::render::RENDER_NAMESPACE
{
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
namespace
{
struct ShadowCacheSlot
{
    const light* owner = nullptr;
    u32 lastFrame = 0;
    LocalShadowStaticCache cache;
};
std::array<ShadowCacheSlot, 256> shadowCacheSlots;
std::array<unsigned, 2> shadowAuditFrames{};

class ShadowSourcesAudit
{
    struct Visual
    {
        int id;
        bool reported = false;
    };
    std::unordered_map<const dxRender_Visual*, Visual> visuals;
    int nextEmbeddedId = -1;

    int Identify(const dxRender_Visual* visual)
    {
        auto found = visuals.find(visual);
        if (found == visuals.end())
            found = visuals.emplace(visual, Visual{nextEmbeddedId--}).first;
        auto& entry = found->second;
        if (!entry.reported)
        {
            entry.reported = true;
            const auto& center = visual->vis.sphere.P;
            Msg("* [gles_shadow_visual] frame=%u visual=%d ptr=%p type=%u center=%.3f,%.3f,%.3f radius=%.3f",
                Device.dwFrame, entry.id, static_cast<const void*>(visual), visual->Type,
                double(center.x), double(center.y), double(center.z),
                double(visual->vis.sphere.R));
        }
        return entry.id;
    }

public:
    explicit ShadowSourcesAudit(const xr_vector<dxRender_Visual*>& levelVisuals)
    {
        visuals.reserve(levelVisuals.size());
        for (size_t i = 0; i < levelVisuals.size(); ++i)
            visuals.emplace(levelVisuals[i], Visual{static_cast<int>(i)});
    }

    void Write(const light& tile, R_dsgraph_structure& graph, bool rendered)
    {
        const light& source = tile.shadow_parent ? *tile.shadow_parent : tile;
        u32 staticEntries, ignored;
        graph.get_Counters(staticEntries, ignored);
        size_t dynamicEntries = 0;
        for (unsigned priority = 0; priority < 2; ++priority)
        {
            auto& map = graph.mapMatrixPasses[priority][0];
            if (!map.empty())
                for (const auto& group : map)
                    dynamicEntries += group.second.size();
        }
        const auto& pos = source.position;
        const auto* projection = reinterpret_cast<const unsigned char*>(&tile.X.S.combine);
        u64 projectionKey = 14695981039346656037ull;
        for (size_t i = 0; i < sizeof(tile.X.S.combine); ++i)
            projectionKey = (projectionKey ^ projection[i]) * 1099511628211ull;
        Msg("* [gles_shadow_source] frame=%u source=%p level_light=%d source_type=%u static=%u face=%d"
            " position=%.3f,%.3f,%.3f range=%.3f sector=%zu atlas=%u tile=%u,%u,%u projection=%016llx rendered=%u"
            " static_entries=%u dynamic_first_pass=%zu sorted_entries=%zu",
            Device.dwFrame, static_cast<const void*>(&source), source.shadow_level_index, unsigned(source.flags.type),
            unsigned(source.flags.bStatic), tile.shadow_face, double(pos.x), double(pos.y), double(pos.z),
            double(source.range), graph.o.sector_id, unsigned(tile.vis.smap_ID), tile.X.S.posX, tile.X.S.posY,
            tile.X.S.size, static_cast<unsigned long long>(projectionKey), unsigned(rendered), staticEntries,
            dynamicEntries, graph.mapSorted.size());

        std::string ids;
        const auto flush = [&]()
        {
            if (!ids.empty())
                Msg("* [gles_shadow_casters] frame=%u source=%p face=%d static_first_pass=%s",
                    Device.dwFrame, static_cast<const void*>(&source), tile.shadow_face, ids.c_str());
            ids.clear();
        };
        for (unsigned priority = 0; priority < 2; ++priority)
        {
            auto& map = graph.mapNormalPasses[priority][0];
            if (map.empty())
                continue;
            for (const auto& group : map)
                for (const auto& item : group.second)
                {
                    if (!ids.empty())
                        ids += ',';
                    ids += std::to_string(Identify(item.pVisual));
                    if (ids.size() >= 600)
                        flush();
                }
        }
        flush();

        const auto writeObject = [&](const auto& item, bool sorted)
        {
            const int id = Identify(item.pVisual);
            const auto& world = item.Matrix.c;
            Msg("* [gles_shadow_object] frame=%u source=%p face=%d object=%p visual=%d sorted=%u world=%.3f,%.3f,%.3f",
                Device.dwFrame, static_cast<const void*>(&source), tile.shadow_face,
                static_cast<const void*>(item.pObject), id, unsigned(sorted),
                double(world.x), double(world.y), double(world.z));
        };
        for (unsigned priority = 0; priority < 2; ++priority)
        {
            auto& map = graph.mapMatrixPasses[priority][0];
            if (!map.empty())
                for (const auto& group : map)
                    for (const auto& item : group.second)
                        writeObject(item, false);
        }
        if (!graph.mapSorted.empty())
            for (const auto& entry : graph.mapSorted)
                writeObject(entry.second, true);
    }
};
}

LocalShadowStaticCache* LocalShadowStaticCache::Acquire(const light* source, u32 frame)
{
    // Acquisition is on the render thread. A slot used in this frame may
    // still be in a build task, so it cannot be reused or evicted this frame.
    const size_t first = (reinterpret_cast<uintptr_t>(source) >> 4) % shadowCacheSlots.size();
    ShadowCacheSlot* replacement = nullptr;
    for (size_t offset = 0; offset < 8; ++offset)
    {
        auto& slot = shadowCacheSlots[(first + offset) % shadowCacheSlots.size()];
        if (slot.owner == source)
        {
            if (slot.lastFrame == frame)
                return nullptr;
            replacement = &slot;
            break;
        }
        if ((!slot.owner || slot.lastFrame != frame) &&
            (!replacement || !slot.owner || (replacement->owner && frame - slot.lastFrame > frame - replacement->lastFrame)))
            replacement = &slot;
    }
    if (!replacement)
        return nullptr;
    if (replacement->owner != source)
    {
        replacement->owner = source;
        replacement->cache = {};
    }
    replacement->lastFrame = frame;
    replacement->cache.result = Result::Bypass;
    replacement->cache.skippedTests = 0;
    return &replacement->cache;
}

void LocalShadowStaticCache::Clear()
{
    for (auto& slot : shadowCacheSlots)
        slot = {};
    shadowAuditFrames = {};
}
#endif

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
u32 gles_spot_shadow_audit_ticket(const light& source);
void gles_spot_shadow_audit_framebuffer(const CBackend& cmd_list, const light& source,
    const CRT& desired_depth, u32 ticket, pcstr stage);
#endif

void CRender::render_lights(light_Package& LP)
{
    ZoneScoped;
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    const bool sampleAtlas = GLGpuProfiler::SamplingShadowAtlas() && !LP.v_shadowed.empty();
    u32 atlasClears = 0, atlasTiles = 0, depthDraws = 0, grassDraws = 0;
    u64 tilePixels = 0;
#endif
    const bool hwOcclusion = HWOCC.is_enabled();
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
    AndroidCpuProfile::Scope localLights(AndroidCpuProfile::Phase::LocalLights);
    struct BuildSample
    {
        AndroidCpuProfile::Timing timing;
        bool onFrameThread = false;
    };
    std::array<BuildSample, R__NUM_PARALLEL_CONTEXTS> buildSamples;
    const bool sampleBuild = AndroidCpuProfile::Sampling();
#endif
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
    GLDetailShadowCull::BoundsBatch detailBoundsBatch(
        Details, GLDetailShadowCull::HasOption(Core.Params, "-gles_detail_cells"));
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    const bool useStaticCache = !hwOcclusion &&
        GLDetailShadowCull::HasOption(Core.Params, "-gles_shadow_static_cache");
    // Optional CPU wall-time diagnostics for individual shadow tiles.
    // No GPU synchronization or changes to rendering.
    const bool sampleShadowLightCost = !LP.v_shadowed.empty() && Device.dwFrame % 67 == 0 &&
        GLDetailShadowCull::HasOption(Core.Params, "-gles_shadow_light_cost");
    const bool sampleShadowCache = useStaticCache && !LP.v_shadowed.empty() && Device.dwFrame % 67 == 0;
    const bool verifyShadowCache = sampleShadowCache &&
        GLDetailShadowCull::HasOption(Core.Params, "-gles_verify_shadow_static_cache");
    u32 cacheBuilt = 0, cacheHits = 0, cacheVerified = 0, cacheMismatch = 0, cacheBypass = 0, testsSkipped = 0;
    std::optional<ShadowSourcesAudit> shadowAudit;
    if (Device.dwFrame % 67 == 0 && !LP.v_shadowed.empty() &&
        GLDetailShadowCull::HasOption(Core.Params, "-gles_shadow_sources"))
    {
        const auto count = LP.v_shadowed.size();
        if (count <= 8 || count >= 30)
        {
            const bool heavy = count >= 30;
            auto& frames = shadowAuditFrames[heavy];
            if (frames < (heavy ? 2u : 1u))
            {
                ++frames;
                shadowAudit.emplace(Visuals);
            }
        }
    }
#endif

    //////////////////////////////////////////////////////////////////////////
    // Refactor order based on ability to pack shadow-maps
    // 1. calculate area + sort in descending order
    // const	u16		smap_unassigned		= u16(-1);
    {
        xr_vector<light*>& source = LP.v_shadowed;
        for (u32 it = 0; it < source.size(); it++)
        {
            light* L = source[it];
            {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    L->vis_update();
}
            if (!L->vis.visible)
            {
                source.erase(source.begin() + it);
                it--;
            }
            else
            {
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    LR.compute_xf_spot(L);
}
            }
        }
    }

#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope packingProbe(GLGpuProfiler::Stage::LocalPack, false);
#endif
    // 2. refactor - infact we could go from the backside and sort in ascending order
    {
        xr_vector<light*>& source = LP.v_shadowed;
        xr_vector<light*> refactored;
        refactored.reserve(source.size());
        const size_t total = source.size();

        for (u16 smap_ID = 0; refactored.size() != total; ++smap_ID)
        {
            LP_smap_pool.initialize(RImplementation.o.smapsize);
            std::sort(source.begin(), source.end(), [](light* l1, light* l2)
            {
                const u32 a0 = l1->X.S.size;
                const u32 a1 = l2->X.S.size;
                return a0 > a1; // reverse -> descending
            });
            for (size_t test = 0; test < source.size(); ++test)
            {
                light* L = source[test];
                SMAP_Rect R{};
                if (LP_smap_pool.push(R, L->X.S.size))
                {
                    // OK
                    L->X.S.posX = R.min.x;
                    L->X.S.posY = R.min.y;
                    L->vis.smap_ID = smap_ID;
                    refactored.push_back(L);
                    source.erase(source.begin() + test);
                    --test;
                }
            }
        }

        // save (lights are popped from back)
        std::reverse(refactored.begin(), refactored.end());
        LP.v_shadowed = std::move(refactored);
    }

#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    packingProbe.Stop();
#endif
    auto& cmd_list = get_imm_context().cmd_list;
    Target->rt_smap_depth->set_slice_read(0);

    PIX_EVENT(SHADOWED_LIGHTS);

    //////////////////////////////////////////////////////////////////////////
    // sort lights by importance???
    // while (has_any_lights_that_cast_shadows) {
    //		if (has_point_shadowed)		->	generate point shadowmap
    //		if (has_spot_shadowed)		->	generate spot shadowmap
    //		switch-to-accumulator
    //		if (has_point_unshadowed)	-> 	accum point unshadowed
    //		if (has_spot_unshadowed)	-> 	accum spot unshadowed
    //		if (was_point_shadowed)		->	accum point shadowed
    //		if (was_spot_shadowed)		->	accum spot shadowed
    //	}
    //	if (left_some_lights_that_doesn't cast shadows)
    //		accumulate them
    static xr_vector<light*> L_spot_s;

    struct task_data_t
    {
        light* L{};
        Task* task{};
        u32 batch_id{};
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
        LocalShadowStaticCache* static_cache{};
#endif
    };
    static xr_vector<task_data_t> lights_queue{};
    lights_queue.reserve(R__NUM_SUN_CASCADES);

    const auto& flush_lights = [&]()
    {
        ZoneScopedN("flush lights");
        for (const auto& entry : lights_queue)
        {
            light* L = entry.L;
            Task* task = entry.task;
            const u32 batch_id = entry.batch_id;
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
            CTimer shadowLightWaitTimer;
            if (sampleShadowLightCost)
                shadowLightWaitTimer.Start();
#endif
            if (task)
                {
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
    AndroidCpuProfile::Scope waitProbe(AndroidCpuProfile::Phase::LocalWait);
#endif
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalTaskWait, false);
#endif
    TaskScheduler->Wait(*task);
}
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
            const float shadowLightWaitMs = sampleShadowLightCost ?
                shadowLightWaitTimer.GetElapsed_sec() * 1000.f : 0.f;
            CTimer shadowLightSubmitTimer;
            if (sampleShadowLightCost)
                shadowLightSubmitTimer.Start();
            if (task && (entry.static_cache || shadowAudit))
                std::atomic_thread_fence(std::memory_order_acquire);
            if (sampleShadowCache)
            {
                using Result = LocalShadowStaticCache::Result;
                const auto result = entry.static_cache ? entry.static_cache->result : Result::Bypass;
                cacheBuilt += result == Result::Built;
                cacheHits += result == Result::Hit;
                cacheVerified += result == Result::Verified;
                cacheMismatch += result == Result::Mismatch;
                cacheBypass += result == Result::Bypass;
                if (entry.static_cache)
                    testsSkipped += entry.static_cache->skippedTests;
            }
#endif
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
            if (sampleBuild)
            {
                // The task's release decrement precedes the relaxed read in Wait.
                if (task)
                    std::atomic_thread_fence(std::memory_order_acquire);
                const auto& sample = buildSamples[batch_id];
                AndroidCpuProfile::Merge(sample.onFrameThread ? AndroidCpuProfile::Phase::LocalBuildMain :
                    AndroidCpuProfile::Phase::LocalBuildWorkers, sample.timing);
            }
#endif

            auto& dsgraph = get_context(batch_id);

            const bool bNormal = !dsgraph.mapNormalPasses[0][0].empty() || !dsgraph.mapMatrixPasses[0][0].empty();
            const bool bSpecial = !dsgraph.mapNormalPasses[1][0].empty() || !dsgraph.mapMatrixPasses[1][0].empty() ||
                !dsgraph.mapSorted.empty();
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
            const u32 shadowLightCallsStart = sampleShadowLightCost ? dsgraph.cmd_list.stat.render.calls : 0;
            u32 shadowLightStaticEntries = 0, shadowLightDiscardedEntries = 0;
            if (sampleShadowLightCost)
                dsgraph.get_Counters(shadowLightStaticEntries, shadowLightDiscardedEntries);
            if (shadowAudit)
                shadowAudit->Write(*L, dsgraph, bNormal || bSpecial);
#endif
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
            static u32 spot_audit_geometry_logged = 0;
            const u32 spot_audit_ticket = spot_audit_geometry_logged < 16 ? gles_spot_shadow_audit_ticket(*L) : 0;
            const u32 spot_audit_calls_before = spot_audit_ticket ? dsgraph.cmd_list.stat.render.calls : 0;
            if (spot_audit_ticket)
            {
                ++spot_audit_geometry_logged;
                Msg("* [gles_spot_shadow_audit] ticket=%u stage=geometry frame=%u light=%p type=%u context=%u "
                    "normal=%u special=%u normal_static=%u normal_dynamic=%u special_static=%u special_dynamic=%u "
                    "sector=%u view_pos=%g,%g,%g projection_finite=%u",
                    spot_audit_ticket, Device.dwFrame, static_cast<const void*>(L), u32(L->flags.type), batch_id,
                    u32(bNormal), u32(bSpecial), u32(!dsgraph.mapNormalPasses[0][0].empty()),
                    u32(!dsgraph.mapMatrixPasses[0][0].empty()), u32(!dsgraph.mapNormalPasses[1][0].empty()),
                    u32(!dsgraph.mapMatrixPasses[1][0].empty()), u32(dsgraph.o.sector_id),
                    dsgraph.o.view_pos.x, dsgraph.o.view_pos.y, dsgraph.o.view_pos.z, u32(_valid(L->X.S.combine)));
            }
#endif
            if (bNormal || bSpecial)
            {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                const u32 callsBeforeDepth = sampleAtlas ? dsgraph.cmd_list.stat.render.calls : 0;
                if (sampleAtlas)
                {
                    ++atlasTiles;
                    tilePixels += u64(L->X.S.size) * L->X.S.size;
                }
#endif
                PIX_EVENT_CTX(dsgraph.cmd_list, SHADOWED_LIGHT);

                Stats.s_merged++;
                L_spot_s.push_back(L);
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalShadowSetup, false);
#endif
    Target->phase_smap_spot(dsgraph.cmd_list, L);
}
                dsgraph.cmd_list.set_xform_world(Fidentity);
                dsgraph.cmd_list.set_xform_view(L->X.S.view);
                dsgraph.cmd_list.set_xform_project(L->X.S.project);
                {
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
                    GLSpotWriterAudit::Pass writer_audit(L, dsgraph.cmd_list.context_id, Device.dwFrame,
                        L->flags.type == IRender_Light::SPOT && L->flags.bShadow && !L->flags.bStatic);
#endif
                    {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalShadowGeometry);
#endif
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
    AndroidCpuProfile::Scope submitProbe(AndroidCpuProfile::Phase::LocalSubmit);
#endif
    dsgraph.render_graph(0);
}
                }
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
                if (spot_audit_ticket)
                {
                    Msg("* [gles_spot_shadow_audit] ticket=%u stage=opaque_draw light=%p draw_calls=%u",
                        spot_audit_ticket, static_cast<const void*>(L),
                        dsgraph.cmd_list.stat.render.calls - spot_audit_calls_before);
                    gles_spot_shadow_audit_framebuffer(dsgraph.cmd_list, *L, *Target->rt_smap_depth,
                        spot_audit_ticket, "after_opaque");
                }
#endif
                if (ps_r2_ls_flags.test(R2FLAG_SUN_DETAILS))
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    const u32 callsBeforeGrass = sampleAtlas ? dsgraph.cmd_list.stat.render.calls : 0;
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                    const u32 callsBeforeDetails = shadowAudit ? dsgraph.cmd_list.stat.render.calls : 0;
#endif
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
                    {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalShadowDetails);
#endif
    Details->RenderShadow(dsgraph.cmd_list, dsgraph.o.view_frustum);
}
#else
                    Details->Render(dsgraph.cmd_list);
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                    if (shadowAudit && dsgraph.cmd_list.stat.render.calls != callsBeforeDetails)
                        Msg("* [gles_shadow_grass] frame=%u source=%p face=%d draws=%u", Device.dwFrame,
                            static_cast<const void*>(L->shadow_parent ? L->shadow_parent : L), L->shadow_face,
                            dsgraph.cmd_list.stat.render.calls - callsBeforeDetails);
#endif
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    if (sampleAtlas)
                        grassDraws += dsgraph.cmd_list.stat.render.calls - callsBeforeGrass;
#endif
                }
                L->X.S.transluent = FALSE;
                if (bSpecial)
                {
                    L->X.S.transluent = TRUE;
                    Target->phase_smap_spot_tsh(dsgraph.cmd_list, L);
                    PIX_EVENT_CTX(dsgraph.cmd_list, SHADOWED_LIGHTS_RENDER_GRAPH);
                    {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalShadowSpecial);
#endif
    dsgraph.render_graph(1);
} // normal level, secondary priority
                    PIX_EVENT_CTX(dsgraph.cmd_list, SHADOWED_LIGHTS_RENDER_SORTED);
                    {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalShadowSpecial);
#endif
    dsgraph.render_sorted();
} // strict-sorted geoms
                }
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                if (sampleAtlas)
                    depthDraws += dsgraph.cmd_list.stat.render.calls - callsBeforeDepth;
#endif
            }
            else
            {
                Stats.s_finalclip++;
            }

            {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalSmapvisEnd, false);
#endif
    L->svis[batch_id].end();
} // NOTE(DX11): occqs are fetched here, this should be done on the imm context only
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
            if (sampleShadowLightCost)
            {
                const light* source = L->shadow_parent ? L->shadow_parent : L;
                const u32 draws = dsgraph.cmd_list.stat.render.calls - shadowLightCallsStart;
                Msg("* [gles_shadow_light_cost] frame=%u source=%p level_light=%d type=%u static=%u face=%d"
                    " sector=%zu atlas=%u tile=%u,%u,%u normal=%u special=%u"
                    " static_entries=%u depth_draws=%u wait_wall_ms=%.3f submit_wall_ms=%.3f",
                    Device.dwFrame, static_cast<const void*>(source), source->shadow_level_index,
                    unsigned(source->flags.type), unsigned(source->flags.bStatic), L->shadow_face,
                    dsgraph.o.sector_id, unsigned(L->vis.smap_ID), L->X.S.posX, L->X.S.posY, L->X.S.size,
                    unsigned(bNormal), unsigned(bSpecial), shadowLightStaticEntries, draws,
                    double(shadowLightWaitMs), double(shadowLightSubmitTimer.GetElapsed_sec() * 1000.f));
            }
#endif
            RImplementation.release_context(batch_id);
        }

        lights_queue.clear();
    };

    while (!LP.v_shadowed.empty())
    {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        GLGpuProfiler::Scope atlasProbe(GLGpuProfiler::Stage::LocalShadowAtlas);
        if (sampleAtlas)
            ++atlasClears;
#endif
        // if (has_spot_shadowed)
        Stats.s_used++;

        // generate spot shadowmap
        {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalShadowSetup, sampleAtlas);
#endif
    Target->phase_smap_spot_clear(cmd_list);
}
        xr_vector<light*>& source = LP.v_shadowed;
        light* L = source.back();
        const u16 sid = L->vis.smap_ID;
        while (true)
        {
            if (source.empty())
                break;
            L = source.back();
            if (L->vis.smap_ID != sid)
                break;

            const auto batch_id = [&]() {
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
                AndroidCpuProfile::Scope allocProbe(AndroidCpuProfile::Phase::LocalContextAlloc);
#endif
                return alloc_context(false);
            }();
            if (batch_id == R_dsgraph_structure::INVALID_CONTEXT_ID)
            {
                VERIFY(!lights_queue.empty());
                flush_lights();
                continue;
            }

            source.pop_back();
            if (hwOcclusion)
                Lights_LastFrame.push_back(L);

            task_data_t data{};
            data.batch_id = batch_id;
            data.L = L;
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
            if (useStaticCache)
                data.static_cache = LocalShadowStaticCache::Acquire(L, Device.dwFrame);
#endif

#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
            auto* buildSample = sampleBuild ? &buildSamples[batch_id] : nullptr;
            if (buildSample)
                *buildSample = {};
#endif
            const auto& calc_lights = [data
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
                , buildSample
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                , verifyShadowCache
#endif
                ]
            {
                ZoneScopedN("calc lights");
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
                if (buildSample)
                    buildSample->onFrameThread = AndroidCpuProfile::Sampling();
                AndroidCpuProfile::Timer buildProbe(buildSample ? &buildSample->timing : nullptr);
#endif
                auto& dsgraph = RImplementation.get_context(data.batch_id);
                {
                    auto* L = data.L;

                    L->svis[data.batch_id].begin();

                    dsgraph.o.phase = PHASE_SMAP;
                    dsgraph.r_pmask(true, RImplementation.o.Tshadows);
                    dsgraph.o.sector_id = L->spatial.sector_id;
                    dsgraph.o.view_pos = L->position;
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                    // The point light contributes only inside its sphere. A caster
                    // wholly outside it cannot block a ray to an illuminated point.
                    const bool boundedPointShadow = L->flags.type == IRender_Light::OMNIPART &&
                        !L->flags.bVolumetric && !ps_r2_ls_flags.test(R2FLAG_GI);
                    dsgraph.o.point_shadow_range_sq = boundedPointShadow ? _sqr(L->range + EPS_L) : 0.f;
#endif
                    dsgraph.o.xform = L->X.S.combine;
                    dsgraph.o.view_frustum.CreateFromMatrix(L->X.S.combine, FRUSTUM_P_ALL & (~FRUSTUM_P_NEAR));

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                    dsgraph.build_subspace(data.static_cache, verifyShadowCache);
#else
                    dsgraph.build_subspace();
#endif
                }
            };

            // calculate
            if (o.mt_calculate)
            {
                data.task = &TaskScheduler->AddTask(calc_lights);
            }
            else
            {
                calc_lights();
            }
            lights_queue.emplace_back(data);
        }
        flush_lights(); // in case if something left
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        atlasProbe.Stop();
#endif

        cmd_list.Invalidate();

        PIX_EVENT(UNSHADOWED_LIGHTS);

        //		switch-to-accumulator
        Target->phase_accumulator(cmd_list);

        PIX_EVENT(POINT_LIGHTS);

        //		if (has_point_unshadowed)	-> 	accum point unshadowed
        if (!LP.v_point.empty())
        {
            light* L2 = LP.v_point.back();
            LP.v_point.pop_back();
            {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    L2->vis_update();
}
            if (L2->vis.visible)
            {
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalPointAccum);
#endif
    Target->accum_point(cmd_list, L2);
}
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalIndirect, false);
#endif
    render_indirect(L2);
}
            }
        }

        PIX_EVENT(SPOT_LIGHTS);

        //		if (has_spot_unshadowed)	-> 	accum spot unshadowed
        if (!LP.v_spot.empty())
        {
            light* L2 = LP.v_spot.back();
            LP.v_spot.pop_back();
            {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    L2->vis_update();
}
            if (L2->vis.visible)
            {
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    LR.compute_xf_spot(L2);
}
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalSpotAccum);
#endif
    Target->accum_spot(cmd_list, L2, false);
}
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalIndirect, false);
#endif
    render_indirect(L2);
}
            }
        }

        PIX_EVENT(SPOT_LIGHTS_ACCUM_VOLUMETRIC);

        //		if (was_spot_shadowed)		->	accum spot shadowed
        if (!L_spot_s.empty())
        {
            PIX_EVENT(ACCUM_SPOT);
            for (light* p_light : L_spot_s)
            {
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                CTimer shadowLightAccumTimer;
                if (sampleShadowLightCost)
                    shadowLightAccumTimer.Start();
#endif
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalSpotAccum);
#endif
    Target->accum_spot(cmd_list, p_light);
}
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalIndirect, false);
#endif
    render_indirect(p_light);
}
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
                if (sampleShadowLightCost)
                {
                    const light* source = p_light->shadow_parent ? p_light->shadow_parent : p_light;
                    Msg("* [gles_shadow_light_accum] frame=%u source=%p face=%d accum_wall_ms=%.3f",
                        Device.dwFrame, static_cast<const void*>(source), p_light->shadow_face,
                        double(shadowLightAccumTimer.GetElapsed_sec() * 1000.f));
                }
#endif
            }

            PIX_EVENT(ACCUM_VOLUMETRIC);
            if (RImplementation.o.advancedpp && ps_r2_ls_flags.is(R2FLAG_VOLUMETRIC_LIGHTS))
                for (light* p_light : L_spot_s)
                    {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVolumetric);
#endif
    Target->accum_volumetric(cmd_list, p_light);
}

            L_spot_s.clear();
        }
    }

    PIX_EVENT(POINT_LIGHTS_ACCUM);
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    if (sampleAtlas)
        Msg("* [gles_shadow_atlas] frame=%u clears=%u tiles=%u tile_pixels=%llu depth_draws=%u grass_draws=%u",
            Device.dwFrame, atlasClears, atlasTiles, static_cast<unsigned long long>(tilePixels), depthDraws, grassDraws);
#endif
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    if (sampleShadowCache)
        Msg("* [gles_shadow_static_cache] frame=%u built=%u hits=%u verified=%u mismatches=%u bypass=%u tests_skipped=%u",
            Device.dwFrame, cacheBuilt, cacheHits, cacheVerified, cacheMismatch, cacheBypass, testsSkipped);
#endif
    // Point lighting (unshadowed, if left)
    if (!LP.v_point.empty())
    {
        xr_vector<light*>& Lvec = LP.v_point;
        for (light* p_light : Lvec)
        {
            {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    p_light->vis_update();
}
            if (p_light->vis.visible)
            {
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalIndirect, false);
#endif
    render_indirect(p_light);
}
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalPointAccum);
#endif
    Target->accum_point(cmd_list, p_light);
}
            }
        }
        Lvec.clear();
    }

    PIX_EVENT(SPOT_LIGHTS_ACCUM);
    // Spot lighting (unshadowed, if left)
    if (!LP.v_spot.empty())
    {
        xr_vector<light*>& Lvec = LP.v_spot;
        for (light* p_light : Lvec)
        {
            {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    p_light->vis_update();
}
            if (p_light->vis.visible)
            {
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalVisibility, false);
#endif
    LR.compute_xf_spot(p_light);
}
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalIndirect, false);
#endif
    render_indirect(p_light);
}
                {
#if defined(USE_OGL) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope lightProbe(GLGpuProfiler::Stage::LocalSpotAccum);
#endif
    Target->accum_spot(cmd_list, p_light, false);
}
            }
        }
        Lvec.clear();
    }
}

void CRender::render_indirect(light* L) const
{
    if (!ps_r2_ls_flags.test(R2FLAG_GI))
        return;

    auto& cmd_list = RImplementation.get_imm_context().cmd_list;

    light LIGEN;
    LIGEN.set_type(IRender_Light::REFLECTED);
    LIGEN.set_shadow(false);
    LIGEN.set_cone(PI_DIV_2 * 2.f);

    const xr_vector<light_indirect>& Lvec = L->indirect;
    if (Lvec.empty())
        return;
    const float LE = L->color.intensity();
    for (auto& LI : Lvec)
    {
        // energy and color
        const float LIE = LE * LI.E;
        if (LIE < ps_r2_GI_clip)
            continue;

        Fvector T{ L->color.r, L->color.g, L->color.b };
        T.mul(LI.E);
        LIGEN.set_color(T.x, T.y, T.z);

        // geometric
        Fvector L_up{ 0, 1, 0 }, L_right;
        if (_abs(L_up.dotproduct(LI.D)) > .99f)
            L_up = { 0, 0, 1 };

        L_right.crossproduct(L_up, LI.D).normalize();
        LIGEN.spatial.sector_id = LI.S;
        LIGEN.set_position(LI.P);
        LIGEN.set_rotation(LI.D, L_right);

        // range
        // dist^2 / range^2 = A - has infinity number of solutions
        // approximate energy by linear fallof Emax / (1 + x) = Emin
        const float Emax = LIE;
        const float Emin = 1.f / 255.f;
        const float x = (Emax - Emin) / Emin;
        if (x < 0.1f)
            continue;
        LIGEN.set_range(x);

        Target->accum_reflected(cmd_list, &LIGEN);
    }
}
} // namespace xray::render::RENDER_NAMESPACE
