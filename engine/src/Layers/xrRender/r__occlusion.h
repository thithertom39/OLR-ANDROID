#pragma once

#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
#include <cstdint>
#endif

namespace xray::render::RENDER_NAMESPACE
{
class CBackend;
class light;

constexpr u32 occq_size_base = 768; // // queue for occlusion queries
constexpr u32 occq_size = 2 * occq_size_base * R__NUM_PARALLEL_CONTEXTS; // // queue for occlusion queries

// must conform to following order of allocation/free
// a(A), a(B), a(C), a(D), ....
// f(A), f(B), f(C), f(D), ....
// a(A), a(B), a(C), a(D), ....
//	this mean:
//		use as litle of queries as possible
//		first try to use queries allocated first
//	assumption:
//		used queries number is much smaller than total count

class R_occlusion
{
private:
    struct Query
    {
#if defined(USE_DX11)
        ID3DQuery* Q;
#elif defined(USE_OGL)
        GLuint Q;
#else
#   error No graphics API selected or enabled!
#endif
        u32 order;
    };

    static constexpr u32 iInvalidHandle = 0xFFFFFFFF;

    bool enabled{ true };
    xr_vector<Query> pool; // sorted (max ... min), insertions are usually at the end
    xr_vector<Query> used; // id's are generated from this and it is cleared from back only
    xr_vector<u32> fids; // free id's

    Lock render_lock{};
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
    // Separate boolean query names. Never mix these with the sample-count
    // consumers (pixel_calculator and smapvis).
    struct LightQuery
    {
        u32 frame;
        u32 lightIndex;
        std::uintptr_t sourceIdentity;
    };
    static constexpr u32 lightQueryCapacity = occq_size_base;
    GLuint lightQueryNames[lightQueryCapacity]{};
    LightQuery lightQueries[lightQueryCapacity]{};
    std::uintptr_t occludedLightIdentities[lightQueryCapacity]{};
    void* lightQueryContext = nullptr;
    u32 lightQueryHead = 0;
    u32 lightQueryCount = 0;
    u32 lightQueryFrame = iInvalidHandle;
    u32 lightQueriesIssued = 0;
    u32 lightQueriesReady = 0;
    u32 lightQueriesOccluded = 0;
    u32 lightLatePolls = 0;
    bool lightQueryActive = false;
    bool lightQueriesEnabled = false;
    bool lightQueriesCull = false;
    bool lightQueriesLateProbe = false;
    bool lightQueriesLateCull = false;
    bool lightLateCullStarted = false;
    bool lightProbeMidSampled = false;
    bool lightProbeEndSampled = false;

    bool occq_light_begin_frame();
    bool occq_light_begin(u32 lightIndex, const light* source);
    void occq_light_end();
    bool occq_light_current() const;
    bool occq_light_available() const;
    void occq_light_retire();
    void occq_light_read_ready(const xr_vector<light*>* candidates);
#endif
public:
#if defined(USE_DX11)
    typedef u64 occq_result;
#elif defined(USE_OGL)
    typedef u32 occq_result;
#else
#   error No graphics API selected or enabled!
#endif
public:
    ~R_occlusion();

    bool is_enabled() const { return enabled; }

    void occq_create(u32 limit);
    void occq_destroy();
    u32 occq_begin(u32& ID); // returns 'order'
    void occq_end(u32& ID);
    occq_result occq_get(u32& ID);
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
    bool is_light_enabled() const { return lightQueriesEnabled; }
    bool is_light_late_probe_enabled() const { return lightQueriesEnabled && lightQueriesLateProbe; }
    void occq_light_issue(CBackend& cmd_list, const xr_vector<light*>& candidates);
    void occq_light_collect(const xr_vector<light*>& candidates, bool logStats);
    void occq_light_probe_late(bool end, u32 shadowPasses);
    bool occq_light_begin_late_cull();
    bool occq_light_poll_late_cull();
    bool occq_light_is_occluded(const light* source) const;
    void occq_light_log_late_cull(size_t candidates, u32 shadowPasses, u32 earlyCulled,
        u32 queuedCulled, u32 queuedPolls);
    void occq_light_clear();
#endif
};
} // namespace xray::render::RENDER_NAMESPACE
