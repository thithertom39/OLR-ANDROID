#include "stdafx.h"
#include "r__occlusion.h"

#include "QueryHelper.h"
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
#include "Layers/xrRender/light.h"
#include "xrEngine/AndroidSceneCompare.h"
#include <SDL.h>
#endif

namespace xray::render::RENDER_NAMESPACE
{
R_occlusion::~R_occlusion(void) { occq_destroy(); }
void R_occlusion::occq_create(u32 limit)
{
    ZoneScoped;
#if defined(USE_OGL)
    // GL ES allows exactly one active query per target, and this code has no
    // guard against opening one while another is still open: light::vis_prepare
    // has no re-entry check on vis.pending, and both smapvis and pixel_calculator
    // call render_graph() with a query already open. The nested glBeginQuery is
    // rejected by the driver, and the matching glEndQuery is then left with
    // nothing to close -- CHK_GL turns that into a fatal GL error.
    // Off by default on GL; pass -occq to opt back in.
    enabled = strstr(Core.Params, "-occq") != nullptr;
#if defined(XR_PLATFORM_ANDROID)
    lightQueriesLateProbe = AndroidSceneCompare::HasOption(Core.Params, "-gles_light_occq_late_probe");
    lightQueriesLateCull = !lightQueriesLateProbe && AndroidSceneCompare::HasOption(Core.Params, "-gles_light_occq_late");
    lightQueriesCull = !lightQueriesLateProbe && !lightQueriesLateCull &&
        AndroidSceneCompare::HasOption(Core.Params, "-gles_light_occq");
    const bool lightQueriesRequested = lightQueriesCull || lightQueriesLateProbe || lightQueriesLateCull ||
        AndroidSceneCompare::HasOption(Core.Params, "-gles_light_occq_probe");
    lightQueriesEnabled = lightQueriesRequested && !enabled && GLAD_GL_ES_VERSION_3_0;
    if (lightQueriesRequested)
        Msg("* [gles_light_occq] enabled=%u culling=%u target=0x%x late_probe=%u late_cull=%u",
            unsigned(lightQueriesEnabled), unsigned(lightQueriesEnabled && lightQueriesCull),
            GL_ANY_SAMPLES_PASSED_CONSERVATIVE, unsigned(is_light_late_probe_enabled()),
            unsigned(lightQueriesEnabled && lightQueriesLateCull));
#endif
#else
    enabled = strstr(Core.Params, "-no_occq") ? false : true;
#endif
    if (!enabled)
        return;

    pool.reserve(limit);
    used.reserve(limit);
    fids.reserve(limit);
    for (u32 it = 0; it < limit; it++)
    {
        Query q;
        q.order = it;
        if (FAILED(CreateQuery(&q.Q, D3D_QUERY_OCCLUSION)))
            break;
        pool.emplace_back(std::move(q));
    }
    std::reverse(pool.begin(), pool.end());
}
void R_occlusion::occq_destroy()
{
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
    occq_light_clear();
    lightQueriesEnabled = false;
#endif
    for (const auto& q : used)
        ReleaseQuery(q.Q);
    for (const auto& q : pool)
        ReleaseQuery(q.Q);
    used.clear();
    pool.clear();
    fids.clear();
}

u32 R_occlusion::occq_begin(u32& ID)
{
    if (!enabled)
    {
        ID = iInvalidHandle;
        return 0;
    }

    ScopeLock lock{ &render_lock };

    if (pool.empty())
    {
        const auto sz = used.size();
        Query q;
        q.order = static_cast<u32>(sz);
        if (FAILED(CreateQuery(&q.Q, D3D_QUERY_OCCLUSION)))
        {
            if ((Device.dwFrame % 40) == 0)
                Msg(" RENDER [Warning]: Too many occlusion queries were issued (>%zu)!!!", sz);
            ID = iInvalidHandle;
            return 0;
        }
        if (sz == used.capacity())
        {
            used.reserve(sz + occq_size_base);
            pool.reserve(sz + occq_size_base);
        }
        pool.emplace(pool.begin(), std::move(q));
    }

    RImplementation.BasicStats.OcclusionQueries++;
    if (!fids.empty())
    {
        ID = fids.back();
        fids.pop_back();
        used[ID] = std::move(pool.back());
    }
    else
    {
        ID = static_cast<u32>(used.size());
        used.emplace_back(std::move(pool.back()));
    }
    pool.pop_back();
    CHK_DX(BeginQuery(used[ID].Q));

    return used[ID].order;
}
void R_occlusion::occq_end(u32& ID)
{
    if (!enabled || ID == iInvalidHandle)
        return;

    ScopeLock lock{ &render_lock };

    CHK_DX(EndQuery(used[ID].Q));
}
R_occlusion::occq_result R_occlusion::occq_get(u32& ID)
{
    if (!enabled || ID == iInvalidHandle)
        return 0xffffffff;

    ScopeLock lock{ &render_lock };

    occq_result fragments = 0;
    HRESULT hr;
    CTimer T;
    T.Start();
    RImplementation.BasicStats.Wait.Begin();
    while ((hr = GetData(used[ID].Q, &fragments, sizeof(fragments))) == S_FALSE)
    {
        if (!SwitchToThread())
            Sleep(ps_r2_wait_sleep);

        if (T.GetElapsed_ms() > 500)
        {
            fragments = (occq_result)-1; // 0xffffffff;
            break;
        }
    }
    RImplementation.BasicStats.Wait.End();

    if (0 == fragments)
        RImplementation.BasicStats.OcclusionCulled++;

    // insert into pool (sorting in decreasing order)
    Query& Q = used[ID];
    if (pool.empty())
        pool.emplace_back(Q);
    else
    {
        int it = int(pool.size()) - 1;
        while ((it >= 0) && (pool[it].order < Q.order))
            it--;
        pool.emplace(pool.begin() + it + 1, std::move(Q));
    }

    // remove from used and shrink as nesessary
    used[ID].Q = 0;
    fids.emplace_back(ID);
    ID = 0;
    return fragments;
}

#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
namespace
{
constexpr GLenum lightQueryTarget = GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
}

void R_occlusion::occq_light_clear()
{
    if (lightQueryContext && SDL_GL_GetCurrentContext() == lightQueryContext)
    {
        if (lightQueryActive)
            CHK_GL(glEndQuery(lightQueryTarget));
        CHK_GL(glDeleteQueries(lightQueryCapacity, lightQueryNames));
    }
    lightQueryContext = nullptr;
    lightQueryActive = false;
    lightQueryHead = lightQueryCount = lightQueriesIssued = 0;
    lightQueriesReady = lightQueriesOccluded = lightLatePolls = 0;
    lightProbeMidSampled = lightProbeEndSampled = lightLateCullStarted = false;
    lightQueryFrame = iInvalidHandle;
}

bool R_occlusion::occq_light_begin_frame()
{
    if (!lightQueriesEnabled)
        return false;
    const auto context = SDL_GL_GetCurrentContext();
    if (context != lightQueryContext)
        occq_light_clear();
    if (!context || lightQueryActive || lightQueryFrame == Device.dwFrame)
        return false;

    lightQueryFrame = Device.dwFrame;
    lightQueriesIssued = 0;
    lightQueriesReady = lightQueriesOccluded = lightLatePolls = 0;
    lightProbeMidSampled = lightProbeEndSampled = lightLateCullStarted = false;
    // GLES treats the precise and conservative targets as one query type.
    GLint active = 0;
    CHK_GL(glGetQueryiv(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &active));
    if (active)
        return false;
    CHK_GL(glGetQueryiv(lightQueryTarget, GL_CURRENT_QUERY, &active));
    if (active)
        return false;

    if (!lightQueryContext)
    {
        CHK_GL(glGenQueries(lightQueryCapacity, lightQueryNames));
        lightQueryContext = context;
    }

    // Reclaim names only after completion. Older frames may still be in
    // flight, but their visibility cannot be used with today's depth buffer.
    while (lightQueryCount && occq_light_available())
        occq_light_retire();
    return true;
}

bool R_occlusion::occq_light_begin(u32 lightIndex, const light* source)
{
    if (lightQueryActive || lightQueryCount == lightQueryCapacity)
        return false;
    const auto slot = (lightQueryHead + lightQueryCount) % lightQueryCapacity;
    if (!lightQueryNames[slot])
        return false;
    CHK_GL(glBeginQuery(lightQueryTarget, lightQueryNames[slot]));
    const auto identity = lightQueriesLateCull ? reinterpret_cast<std::uintptr_t>(source) : 0;
    lightQueries[slot] = { lightQueryFrame, lightIndex, identity };
    lightQueryActive = true;
    return true;
}

void R_occlusion::occq_light_end()
{
    if (!lightQueryActive)
        return;
    CHK_GL(glEndQuery(lightQueryTarget));
    lightQueryActive = false;
    ++lightQueryCount;
    ++lightQueriesIssued;
}

bool R_occlusion::occq_light_available() const
{
    GLuint available = GL_FALSE;
    CHK_GL(glGetQueryObjectuiv(lightQueryNames[lightQueryHead], GL_QUERY_RESULT_AVAILABLE, &available));
    return available == GL_TRUE;
}

bool R_occlusion::occq_light_current() const
{
    return lightQueriesIssued && lightQueryFrame == Device.dwFrame &&
        !lightQueryActive && lightQueryContext == SDL_GL_GetCurrentContext();
}

void R_occlusion::occq_light_retire()
{
    lightQueryHead = (lightQueryHead + 1) % lightQueryCapacity;
    --lightQueryCount;
}

void R_occlusion::occq_light_read_ready(const xr_vector<light*>* candidates)
{
    const u32 previouslyOccluded = lightQueriesOccluded;
    while (lightQueryCount && occq_light_available())
    {
        const auto& entry = lightQueries[lightQueryHead];
        if (entry.frame == Device.dwFrame && (!candidates || entry.lightIndex < candidates->size()))
        {
            GLuint visible = GL_TRUE;
            CHK_GL(glGetQueryObjectuiv(lightQueryNames[lightQueryHead], GL_QUERY_RESULT, &visible));
            ++lightQueriesReady;
            if (visible == GL_FALSE)
            {
                if (lightQueriesLateCull)
                    occludedLightIdentities[lightQueriesOccluded] = entry.sourceIdentity;
                if (lightQueriesCull && candidates)
                    (*candidates)[entry.lightIndex]->vis.visible = false;
                ++lightQueriesOccluded;
            }
        }
        occq_light_retire();
    }
    if (lightQueriesLateCull && lightQueriesOccluded != previouslyOccluded)
        std::sort(occludedLightIdentities, occludedLightIdentities + lightQueriesOccluded);
}

void R_occlusion::occq_light_collect(const xr_vector<light*>& candidates, bool logStats)
{
    const bool sameFrame = occq_light_current();
    if (sameFrame)
        occq_light_read_ready(&candidates);
    if (logStats)
    {
        const u32 issued = sameFrame ? lightQueriesIssued : 0;
        const u32 ready = sameFrame ? lightQueriesReady : 0;
        const u32 occluded = sameFrame ? lightQueriesOccluded : 0;
        Msg("* [gles_light_occq] frame=%u candidates=%zu issued=%u ready=%u pending=%u occluded=%u culled=%u",
            Device.dwFrame, candidates.size(), issued, ready, issued - ready,
            occluded, lightQueriesCull ? occluded : 0);
    }
    if (!lightQueriesLateProbe && !lightQueriesLateCull)
        lightQueriesIssued = 0;
}

void R_occlusion::occq_light_probe_late(bool end, u32 shadowPasses)
{
    if (!is_light_late_probe_enabled() || !occq_light_current())
        return;
    auto& sampled = end ? lightProbeEndSampled : lightProbeMidSampled;
    if (sampled || lightProbeEndSampled)
        return;
    sampled = true;

    // Atlas packing reordered the candidates. Only count the results for
    // this frame and retire finished names, without consulting candidate IDs.
    occq_light_read_ready(nullptr);
    Msg("* [gles_light_occq_late] frame=%u stage=%s shadow_passes=%u issued=%u ready=%u pending=%u occluded=%u",
        Device.dwFrame, end ? "end" : "mid", shadowPasses, lightQueriesIssued,
        lightQueriesReady, lightQueriesIssued - lightQueriesReady, lightQueriesOccluded);
}

bool R_occlusion::occq_light_begin_late_cull()
{
    if (!lightQueriesEnabled || !lightQueriesLateCull || lightLateCullStarted || !occq_light_current())
        return false;
    lightLateCullStarted = true;
    return true;
}

bool R_occlusion::occq_light_poll_late_cull()
{
    if (!lightLateCullStarted || lightQueriesReady == lightQueriesIssued || !occq_light_current())
        return false;
    ++lightLatePolls;
    occq_light_read_ready(nullptr);
    return true;
}

bool R_occlusion::occq_light_is_occluded(const light* source) const
{
    if (!lightLateCullStarted || !lightQueriesOccluded || !occq_light_current())
        return false;
    // Addresses are used only as current-frame keys. Pending results can
    // survive level changes in the driver, but never trigger a dereference.
    return std::binary_search(occludedLightIdentities, occludedLightIdentities + lightQueriesOccluded,
        reinterpret_cast<std::uintptr_t>(source));
}

void R_occlusion::occq_light_log_late_cull(size_t candidates, u32 shadowPasses, u32 earlyCulled,
    u32 queuedCulled, u32 queuedPolls)
{
    if (!lightLateCullStarted || !occq_light_current())
        return;
    Msg("* [gles_light_occq_late_cull] frame=%u candidates=%zu shadow_passes=%u issued=%u ready=%u "
        "pending=%u occluded=%u culled=%u polls=%u early_culled=%u queued_culled=%u queued_polls=%u",
        Device.dwFrame, candidates, shadowPasses,
        lightQueriesIssued, lightQueriesReady, lightQueriesIssued - lightQueriesReady, lightQueriesOccluded,
        earlyCulled + queuedCulled, lightLatePolls, earlyCulled, queuedCulled, queuedPolls);
}
#endif
} // namespace xray::render::RENDER_NAMESPACE
