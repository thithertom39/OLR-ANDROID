#include "stdafx.h"
#include "glGpuProfiler.h"

#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "xrEngine/IGame_Persistent.h"
#if defined(XR_PLATFORM_ANDROID)
#include "glStaticIndexBatch.h"
#endif
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace xray::render::RENDER_NAMESPACE::GLGpuProfiler
{
namespace
{
constexpr unsigned StageCount = unsigned(Stage::Count);
constexpr unsigned RingSize = 8;
constexpr unsigned EventCapacity = 512;
constexpr unsigned QueryStride = EventCapacity * 2;
constexpr unsigned InvalidEvent = ~0u;
constexpr unsigned SampleStride = 32;
constexpr unsigned AtlasSampleStride = 127;
constexpr unsigned IssueLogLimit = 16;
constexpr GLuint64 UnwrittenQuery = ~GLuint64(0);
constexpr GLuint64 MaxTimerNs = 1000000000ULL;
// Per-light CPU scopes remain available, but querying every shadow subpass
// exhausts the event ring in scenes with dozens of lights and perturbs them.
// The enclosing local_lights and local_shadow_atlas retain GPU timestamps.
bool CpuOnlyLocalStage(Stage stage)
{
    return stage == Stage::LocalShadowGeometry || stage == Stage::LocalShadowDetails ||
        stage == Stage::LocalShadowSpecial || stage == Stage::LocalPointAccum ||
        stage == Stage::LocalSpotAccum || stage == Stage::LocalVolumetric;
}
constexpr const char* Names[StageCount] = {
    "render_total", "world", "main_geometry", "details_all_submissions",
    "sun_shadow_and_direct", "local_lights", "combine", "forward_world",
    "bloom_luminance_inclusive", "luminance", "final_combine_aa", "extra_pp", "present_blit",
    "z_prefill", "sync_wait", "scene_prepare", "light_visibility", "wallmarks_hud_ui",
    "rain", "emissive", "light_flush", "calculate", "hom_wait",
    "local_normal", "local_pending", "local_visibility_cpu", "local_pack_cpu", "local_task_wait_cpu", "local_shadow_setup", "local_shadow_geometry", "local_shadow_details", "local_shadow_special", "local_smapvis_end", "local_point_accum", "local_spot_accum", "local_indirect", "local_volumetric", "sun_shadow_geometry", "sun_details", "sun_special", "sun_accum", "local_shadow_atlas"
};

struct Event
{
    Stage stage{};
    bool ended = false;
};
struct Packet
{
    std::array<Event, EventCapacity> events{};
    std::array<bool, StageCount> overflowedStages{};
    unsigned count = 0;
    unsigned lastQuery = 0;
    unsigned epoch = 0;
    std::uint64_t frame = 0;
    unsigned deviceFrame = 0;
    bool pending = false;
};
struct State
{
    bool enabled = false;
    bool atlasOnly = false;
    bool localElapsed = false;
    bool gpu = false;
    bool inFrame = false;
    bool gpuEnded = false;
    bool localElapsedActive = false;
    IGame_Level* level = nullptr;
    GLint bits = 0;
    unsigned queryStride = QueryStride;
    unsigned epoch = 1;
    unsigned sequence = 0;
    unsigned nextSlot = 0;
    int slot = -1;
    unsigned totalEvent = InvalidEvent;
    unsigned pendingDropped = 0;
    unsigned disjointDropped = 0;
    unsigned invalidDropped = 0;
    unsigned partialPackets = 0;
    unsigned issueRecords = 0;
    std::array<bool, StageCount> issueStageLogged{};
    std::uint64_t frame = 0;
    unsigned deviceFrame = 0;
    std::uint64_t start = 0;
    std::uint64_t threadStart = 0;
    std::uint64_t previousStart = 0;
    std::uint64_t lastFlush = 0;
    double intervalMs = -1;
    unsigned draws = 0;
    unsigned staticBatches = 0, batchedDraws = 0, batchBytes = 0;
    unsigned vertexBindings = 0;
    unsigned legacyVertexBindings = 0;
    std::array<unsigned, 32> mainVisuals{}, shadowVisuals{};
    unsigned detailInstances = 0;
    unsigned shadowDetailInstances = 0;
    LocalDetailCullStats localDetails;
    std::array<std::uint64_t, StageCount> wallNs{};
    std::array<std::uint64_t, StageCount> threadNs{};
    std::array<bool, StageCount> threadInvalid{};
    std::array<unsigned, StageCount> invocations{};
    std::array<unsigned, StageCount> depth{};
    std::array<GLuint, RingSize * QueryStride> queries{};
    std::array<Packet, RingSize> packets{};
    IWriter* writer = nullptr;
};
State state;
// RenderBegin may execute repeatedly without advancing Device.dwFrame while
// loading. Keep a process-local render sequence, including across context reset.
std::uint64_t nextFrame = 0;

std::uint64_t Now()
{
    return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::uint64_t ThreadNow()
{
#if defined(CLOCK_THREAD_CPUTIME_ID)
    timespec t{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) == 0)
        return std::uint64_t(t.tv_sec) * 1000000000ULL + std::uint64_t(t.tv_nsec);
#endif
    return 0;
}

bool Gameplay()
{
    return g_pGameLevel && g_pGameLevel->bReady && Device.b_is_Active && !Device.Paused() &&
        !Device.dwPrecacheFrame && g_loading_events.empty() && !GEnv.isDedicatedServer &&
        (!g_pGamePersistent || !g_pGamePersistent->IsMainMenuActive());
}

template<size_t N>
bool Requested(const char* args, const char (&flag)[N])
{
    if (!args)
        return false;
    for (const char* p = args; (p = std::strstr(p, flag)) != nullptr; ++p)
        if ((p == args || std::strchr(" \t\r\n", p[-1])) &&
            std::strchr(" \t\r\n", p[N - 1]))
            return true;
    return false;
}

void Write(const char* text)
{
    if (state.writer)
        state.writer->w(text, std::strlen(text));
}

#if defined(XR_PLATFORM_ANDROID)
int WriteBatchCounters(char* text, size_t size, const GLStaticIndexBatch::Counters& counters)
{
    return std::snprintf(text, size,
        "{\"hits\":%u,\"misses\":%u,\"uploads\":%u,\"rejected\":%u,\"budget_skips\":%u,\"upload_bytes\":%llu,"
        "\"lookup_cpu_ms\":%.6f,\"convert_cpu_ms\":%.6f,\"upload_cpu_ms\":%.6f}",
        counters.hits, counters.misses, counters.uploads, counters.rejected, counters.budgetSkips,
        static_cast<unsigned long long>(counters.uploadBytes),
        double(counters.lookupNs) * 1e-6, double(counters.convertNs) * 1e-6, double(counters.uploadNs) * 1e-6);
}
#endif

void DropDisjoint()
{
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (!disjoint)
        return;
    ++state.epoch;
    for (auto& packet : state.packets)
    {
        if (packet.pending)
            ++state.disjointDropped;
        packet.pending = false;
    }
    state.slot = -1;
}

struct TimerIssue
{
    const char* reason = nullptr;
    GLuint64 begin = 0;
    GLuint64 end = 0;
};

TimerIssue ReadElapsed(GLuint query)
{
    GLuint64 elapsed = UnwrittenQuery;
    glGetQueryObjectui64vEXT(query, GL_QUERY_RESULT_EXT, &elapsed);
    if (elapsed == UnwrittenQuery)
        return {"query_result_unwritten"};
    if (!elapsed)
        return {"elapsed_zero"};
    if (elapsed > MaxTimerNs)
        return {"elapsed_gt_1s", 0, elapsed};
    return {nullptr, 0, elapsed};
}

void WriteIssue(const Packet& packet, unsigned stage, const TimerIssue& issue)
{
    if (state.issueRecords >= IssueLogLimit || state.issueStageLogged[stage])
        return;
    state.issueStageLogged[stage] = true;
    ++state.issueRecords;
    char row[300];
    std::snprintf(row, sizeof(row),
        "{\"record\":\"gpu_issue\",\"frame\":%llu,\"device_frame\":%u,\"reason\":\"%s\","
        "\"stage\":\"%s\",\"events\":%u,\"begin_ns\":%llu,\"end_ns\":%llu}\n",
        static_cast<unsigned long long>(packet.frame), packet.deviceFrame, issue.reason,
        Names[stage], packet.count,
        static_cast<unsigned long long>(issue.begin), static_cast<unsigned long long>(issue.end));
    Write(row);
}

// Result availability is polled only for an old packet, never in a wait loop.
void Poll()
{
    if (!state.gpu)
        return;
    unsigned oldest = RingSize;
    for (unsigned i = 0; i < RingSize; ++i)
        if (state.packets[i].pending &&
            (oldest == RingSize || state.packets[i].frame < state.packets[oldest].frame))
            oldest = i;
    if (oldest == RingSize)
        return;
    auto& packet = state.packets[oldest];
    if (state.frame < packet.frame || state.frame - packet.frame < 3)
        return;
    GLuint available = GL_FALSE;
    glGetQueryObjectuivEXT(state.queries[oldest * state.queryStride + packet.lastQuery],
        GL_QUERY_RESULT_AVAILABLE_EXT, &available);
    if (!available)
        return;

    // The elapsed packet owns a single query. For timestamp packets the last
    // timestamp covers earlier ones of the same target.
    DropDisjoint();
    if (!packet.pending)
        return;
    std::array<double, StageCount> milliseconds{};
    std::array<unsigned, StageCount> completed{};
    std::array<TimerIssue, StageCount> issues{};
    for (unsigned i = 0; i < StageCount; ++i)
        if (packet.overflowedStages[i])
            issues[i] = {"event_capacity"};
    GLuint64 frameBegin = 0, frameEnd = 0;
    for (unsigned i = 0; i < packet.count; ++i)
    {
        const auto& event = packet.events[i];
        GLuint64 begin = UnwrittenQuery, end = UnwrittenQuery;
        const char* failure = "unclosed_scope";
        if (event.ended)
        {
            const auto* queries = state.queries.data() + oldest * state.queryStride;
            if (state.localElapsed)
            {
                const auto result = ReadElapsed(queries[0]);
                begin = result.begin;
                end = result.end;
                failure = result.reason;
            }
            else
            {
                glGetQueryObjectui64vEXT(queries[i * 2], GL_QUERY_RESULT_EXT, &begin);
                glGetQueryObjectui64vEXT(queries[i * 2 + 1], GL_QUERY_RESULT_EXT, &end);
                if (begin == UnwrittenQuery || end == UnwrittenQuery)
                    failure = "query_result_unwritten";
                else if (begin == 0 || end == 0)
                    failure = "timestamp_zero";
                else if (end < begin)
                    failure = "timestamp_reversed";
                else if (end - begin > MaxTimerNs)
                    failure = "timestamp_interval_gt_1s";
                // RenderTotal is issued first and ended last in timestamp mode.
                // Atlas-only profiling has no enclosing whole-frame timestamps.
                else if (frameEnd && (begin < frameBegin || end > frameEnd))
                    failure = "timestamp_outside_frame";
                else
                    failure = nullptr;
            }
        }
        const auto stage = unsigned(event.stage);
        if (failure)
        {
            if (!issues[stage].reason)
                issues[stage] = {failure, begin == UnwrittenQuery ? 0 : begin, end == UnwrittenQuery ? 0 : end};
        }
        else
        {
            if (event.stage == Stage::RenderTotal)
            {
                frameBegin = begin;
                frameEnd = end;
            }
            ++completed[stage];
            milliseconds[stage] += double(end - begin) * 1e-6;
        }
    }
    DropDisjoint();
    if (!packet.pending)
        return;
    packet.pending = false;
    bool complete = true;
    bool hasValidStage = false;
    for (unsigned i = 0; i < StageCount; ++i)
    {
        if (issues[i].reason)
        {
            complete = false;
            WriteIssue(packet, i, issues[i]);
        }
        else if (completed[i])
            hasValidStage = true;
    }
    if (!hasValidStage)
    {
        ++state.invalidDropped;
        return;
    }
    if (!complete)
        ++state.partialPackets;
    char row[8192];
    int used = std::snprintf(row, sizeof(row),
        "{\"record\":\"gpu\",\"frame\":%llu,\"device_frame\":%u,\"epoch\":%u,\"events\":%u,\"complete\":%s,\"inclusive_ms\":[",
        static_cast<unsigned long long>(packet.frame), packet.deviceFrame, packet.epoch, packet.count,
        complete ? "true" : "false");
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%.6f", i ? "," : "",
            completed[i] && !issues[i].reason ? milliseconds[i] : -1.0);
    std::snprintf(row + used, sizeof(row) - size_t(used), "]}\n");
    Write(row);
}

unsigned StartEvent(Stage stage)
{
    if (state.slot < 0 || state.gpuEnded || state.localElapsed)
        return InvalidEvent;
    auto& packet = state.packets[unsigned(state.slot)];
    if (packet.count == state.queryStride / 2)
    {
        packet.overflowedStages[unsigned(stage)] = true;
        return InvalidEvent;
    }
    const unsigned event = packet.count++;
    packet.events[event] = {stage, false};
    packet.lastQuery = event * 2;
    glQueryCounterEXT(state.queries[unsigned(state.slot) * state.queryStride + packet.lastQuery], GL_TIMESTAMP_EXT);
    return event;
}

void EndEvent(unsigned event)
{
    if (event == InvalidEvent || state.slot < 0 || state.gpuEnded)
        return;
    auto& packet = state.packets[unsigned(state.slot)];
    if (event >= packet.count)
        return;
    const unsigned query = event * 2 + 1;
    glQueryCounterEXT(state.queries[unsigned(state.slot) * state.queryStride + query], GL_TIMESTAMP_EXT);
    packet.events[event].ended = true;
    packet.lastQuery = query;
}

void CloseLocalLightsElapsed(bool complete)
{
    if (!state.localElapsedActive)
        return;
    glEndQueryEXT(GL_TIME_ELAPSED_EXT);
    state.localElapsedActive = false;
    if (state.slot >= 0)
        state.packets[unsigned(state.slot)].events[0].ended = complete;
}
} // namespace

void Shutdown(bool contextCurrent)
{
    if (state.writer)
    {
        state.writer->flush();
        FS.w_close(state.writer);
    }
    if (contextCurrent && state.gpu && glDeleteQueriesEXT)
    {
        CloseLocalLightsElapsed(false);
        glDeleteQueriesEXT(GLsizei(RingSize * state.queryStride), state.queries.data());
    }
    state = State{};
}

void Initialize()
{
    // The previous context may already be gone. Never delete its names in a
    // newly created context; normal shutdown/reset deletes while current.
    Shutdown(false);
    state.localElapsed = Requested(Core.Params, "-gles_gpu_profile_local_elapsed");
    state.atlasOnly = !state.localElapsed && Requested(Core.Params, "-gles_shadow_atlas_profile");
    state.enabled = state.atlasOnly || state.localElapsed || Requested(Core.Params, "-gles_gpu_profile");
    if (!state.enabled)
        return;
    if (state.localElapsed)
        state.queryStride = 1;
    else if (state.atlasOnly)
        state.queryStride = 32;
    const unsigned stride = state.atlasOnly ? AtlasSampleStride : SampleStride;
    const unsigned eventCapacity = state.localElapsed ? 1 : state.queryStride / 2;
    const char* timer = state.localElapsed ? "time_elapsed_local_lights" : "timestamp_pairs";
    const auto target = state.localElapsed ? GL_TIME_ELAPSED_EXT : GL_TIMESTAMP_EXT;
    state.gpu = GLAD_GL_EXT_disjoint_timer_query && glGenQueriesEXT && glDeleteQueriesEXT &&
        glGetQueryObjectuivEXT && glGetQueryObjectui64vEXT && glGetQueryivEXT &&
        (state.localElapsed ? glBeginQueryEXT && glEndQueryEXT : glQueryCounterEXT != nullptr);
    if (state.gpu)
    {
        glGetQueryivEXT(target, GL_QUERY_COUNTER_BITS_EXT, &state.bits);
        state.gpu = state.bits > 0;
    }
    if (state.gpu)
    {
        glGenQueriesEXT(GLsizei(RingSize * state.queryStride), state.queries.data());
        // Treat failed allocation as unavailable; do not abort the renderer.
        for (unsigned i = 0; i < RingSize * state.queryStride; ++i)
            if (!state.queries[i])
                state.gpu = false;
        if (!state.gpu)
            glDeleteQueriesEXT(GLsizei(RingSize * state.queryStride), state.queries.data());
        else
            DropDisjoint();
    }
    char name[96];
    std::snprintf(name, sizeof(name), "gpu-profile-%llu.jsonl", static_cast<unsigned long long>(Now()));
    state.writer = FS.w_open("$logs$", name);
    Msg("* [gles_gpu_profile] diagnostic=on gpu_timestamps=%u gpu_elapsed=%u counter_bits=%d sample_stride=%u event_capacity=%u file=%s gameplay_only=1 cpu_and_gpu_sampled=1 buffered_flush=5s",
        unsigned(state.gpu && !state.localElapsed), unsigned(state.gpu && state.localElapsed),
        state.bits, stride, eventCapacity, state.writer ? name : "unavailable");
    char header[8192];
    int used = std::snprintf(header, sizeof(header), "{\"record\":\"metadata\",\"schema\":4,\"gameplay_only\":true,\"cpu_and_gpu_sampled\":true,\"timing\":\"wall includes waits; thread_cpu excludes descheduling; gpu timeline excludes swap; nested stages overlap; unobserved or incomplete stage is -1\",\"gpu_available\":%s,\"gpu_timer\":\"%s\",\"counter_bits\":%d,\"sample_stride\":%u,\"event_capacity\":%u,\"frame_id\":\"process_render_sequence\",\"stages\":[",
        state.gpu ? "true" : "false", timer, state.bits, stride, eventCapacity);
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(header + used, sizeof(header) - size_t(used), "%s\"%s\"", i ? "," : "", Names[i]);
    std::snprintf(header + used, sizeof(header) - size_t(used), "]}\n");
    Write(header);
    state.lastFlush = Now();
}

void BeginFrame()
{
    if (!state.enabled)
        return;
    state.frame = nextFrame++;
    state.deviceFrame = Device.dwFrame;
    Poll();
    if (!Gameplay())
    {
        // Flush on leaving gameplay too: a short capture must survive a menu/export.
        if (state.previousStart && state.writer)
            state.writer->flush();
        state.previousStart = 0;
        state.level = nullptr;
        return;
    }
    if (state.level != g_pGameLevel)
        state.previousStart = 0;
    state.level = g_pGameLevel;
    const auto start = Now();
    state.intervalMs = state.previousStart && start >= state.previousStart ?
        double(start - state.previousStart) * 1e-6 : -1.0;
    state.previousStart = start;
    // Match CPU and GPU sampling. Unsampled frames do no stage clocks, draw
    // counting, formatting or file writes, even if GPU timers are unavailable.
    if (state.sequence++ % (state.atlasOnly ? AtlasSampleStride : SampleStride))
        return;
    state.inFrame = true;
    state.gpuEnded = false;
    state.slot = -1;
    state.totalEvent = InvalidEvent;
    state.start = start;
    state.threadStart = ThreadNow();
    state.draws = state.detailInstances = state.shadowDetailInstances = 0;
    state.staticBatches = state.batchedDraws = state.batchBytes = 0;
    state.vertexBindings = state.legacyVertexBindings = 0;
    state.mainVisuals = {};
    state.shadowVisuals = {};
    state.localDetails = {};
    state.wallNs = {};
    state.threadNs = {};
    state.threadInvalid = {};
    state.invocations = {};
    state.invocations[unsigned(Stage::RenderTotal)] = 1;
    state.depth = {};
    if (!state.gpu)
        return;
    DropDisjoint();
    unsigned slot = RingSize;
    for (unsigned i = 0; i < RingSize; ++i)
    {
        const unsigned candidate = (state.nextSlot + i) % RingSize;
        if (!state.packets[candidate].pending)
        {
            slot = candidate;
            break;
        }
    }
    if (slot == RingSize)
    {
        ++state.pendingDropped;
        return;
    }
    state.nextSlot = (slot + 1) % RingSize;
    state.packets[slot] = Packet{};
    state.packets[slot].frame = state.frame;
    state.packets[slot].deviceFrame = state.deviceFrame;
    state.packets[slot].epoch = state.epoch;
    state.slot = int(slot);
    if (!state.atlasOnly && !state.localElapsed)
        state.totalEvent = StartEvent(Stage::RenderTotal);
}

void BeginLocalLightsElapsed()
{
    if (!state.localElapsed || state.slot < 0 || state.gpuEnded)
        return;
    auto& packet = state.packets[unsigned(state.slot)];
    if (packet.count)
        return;
    // Respect an elapsed query owned by another profiler. No nested or
    // timestamp queries are issued in this capture mode.
    GLint current = 0;
    glGetQueryivEXT(GL_TIME_ELAPSED_EXT, GL_CURRENT_QUERY_EXT, &current);
    if (current)
        return;
    glBeginQueryEXT(GL_TIME_ELAPSED_EXT, state.queries[unsigned(state.slot)]);
    packet.events[0] = {Stage::LocalLights, false};
    packet.count = 1;
    state.localElapsedActive = true;
}

void EndLocalLightsElapsed()
{
    CloseLocalLightsElapsed(true);
}

void EndGpuFrame()
{
    if (!state.inFrame || state.gpuEnded)
        return;
    CloseLocalLightsElapsed(false);
    EndEvent(state.totalEvent);
    if (state.slot >= 0)
        state.packets[unsigned(state.slot)].pending = state.packets[unsigned(state.slot)].count != 0;
    state.gpuEnded = true;
}

void FinishFrame()
{
    if (!state.inFrame)
        return;
    EndGpuFrame();
    const auto end = Now();
    const auto cpu = ThreadNow();
    const double wallMs = end >= state.start ? double(end - state.start) * 1e-6 : -1.0;
    const double cpuMs = state.threadStart && cpu >= state.threadStart ? double(cpu - state.threadStart) * 1e-6 : -1.0;
    state.inFrame = false;
    if (!Gameplay() || state.level != g_pGameLevel)
    {
        if (state.slot >= 0)
        {
            state.packets[unsigned(state.slot)].pending = false;
            ++state.invalidDropped;
        }
        state.slot = -1;
        state.previousStart = 0;
        return;
    }
    state.slot = -1;
    if (end >= state.start)
        state.wallNs[unsigned(Stage::RenderTotal)] = end - state.start;
    state.threadInvalid[unsigned(Stage::RenderTotal)] = cpuMs < 0;
    if (cpuMs >= 0)
        state.threadNs[unsigned(Stage::RenderTotal)] = cpu - state.threadStart;
    char row[8192];
    int used = std::snprintf(row, sizeof(row),
        "{\"record\":\"cpu\",\"frame\":%llu,\"device_frame\":%u,\"width\":%u,\"height\":%u,\"camera\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],\"interval_ms\":%.6f,\"render_wall_ms\":%.6f,\"render_thread_cpu_ms\":%.6f,\"draw_calls\":%u,\"detail_instances_submitted\":%u,\"sun_detail_instances_submitted\":%u,\"vertex_binding_updates\":%u,\"legacy_vertex_pointer_batches\":%u,\"inclusive_wall_ms\":[",
        static_cast<unsigned long long>(state.frame), state.deviceFrame, Device.dwWidth, Device.dwHeight,
        double(Device.vCameraPosition.x), double(Device.vCameraPosition.y), double(Device.vCameraPosition.z),
        double(Device.vCameraDirection.x), double(Device.vCameraDirection.y), double(Device.vCameraDirection.z),
        state.intervalMs, wallMs, cpuMs, state.draws, state.detailInstances, state.shadowDetailInstances,
        state.vertexBindings, state.legacyVertexBindings);
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%.6f", i ? "," : "", double(state.wallNs[i]) * 1e-6);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), "],\"inclusive_thread_cpu_ms\":[");
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%.6f", i ? "," : "",
            state.threadInvalid[i] ? -1.0 : double(state.threadNs[i]) * 1e-6);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), "],\"logical_stage_invocations\":[");
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%u", i ? "," : "", state.invocations[i]);
    used += std::snprintf(row + used, sizeof(row) - size_t(used),
        "],\"static_batches\":%u,\"batched_draws\":%u,\"batch_bytes\":%u,\"logical_draws\":%u,\"main_visual_submissions\":[",
        state.staticBatches, state.batchedDraws, state.batchBytes,
        state.draws + state.batchedDraws - state.staticBatches);
    for (unsigned i = 0; i < state.mainVisuals.size(); ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%u", i ? "," : "", state.mainVisuals[i]);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), "],\"shadow_visual_submissions\":[");
    for (unsigned i = 0; i < state.shadowVisuals.size(); ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%u", i ? "," : "", state.shadowVisuals[i]);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), "]");
#if defined(XR_PLATFORM_ANDROID)
    const auto batch = GLStaticIndexBatch::FrameStats(state.deviceFrame);
    used += std::snprintf(row + used, sizeof(row) - size_t(used),
        ",\"static_index_cache\":{\"main_enabled\":%s,\"shadow_enabled\":%s,\"resident_bytes\":%zu,"
        "\"resident_entries\":%zu,\"retained_source_bytes\":%zu,\"main\":",
        GLStaticIndexBatch::MainEnabled() ? "true" : "false", GLStaticIndexBatch::ShadowEnabled() ? "true" : "false",
        batch.residentBytes, batch.residentEntries, batch.retainedSourceBytes);
    used += WriteBatchCounters(row + used, sizeof(row) - size_t(used), batch.main);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), ",\"shadow\":");
    used += WriteBatchCounters(row + used, sizeof(row) - size_t(used), batch.shadow);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), "}");
#endif
    const auto& details = state.localDetails;
    std::snprintf(row + used, sizeof(row) - size_t(used),
        ",\"local_detail_cull\":{\"model_tests\":%u,\"models_rejected\":%u,"
        "\"model_bounds_built\":%u,\"cell_bounds_built\":%u,\"cells_tested\":%u,\"cells_rejected\":%u,"
        "\"instance_candidates\":%u,\"instances_skipped_by_cells\":%u,"
        "\"instance_tests\":%u,\"local_instances_submitted\":%u,"
        "\"cell_reference_tests\":%u,\"cell_reference_mismatches\":%u}}\n",
        details.modelTests, details.modelsRejected, details.modelBoundsBuilt, details.cellBoundsBuilt,
        details.cellsTested, details.cellsRejected, details.instanceCandidates, details.instancesSkipped,
        details.instanceCandidates - details.instancesSkipped, details.instancesSubmitted,
        details.cellReferenceTests, details.cellReferenceMismatches);
    Write(row);
    if (end - state.lastFlush >= 5000000000ULL)
    {
        char drop[224];
        std::snprintf(drop, sizeof(drop), "{\"record\":\"gpu_drop\",\"frame\":%llu,\"ring_full_drops\":%u,"
            "\"disjoint_drops\":%u,\"invalid_drops\":%u,\"partial_packets\":%u}\n",
            static_cast<unsigned long long>(state.frame), state.pendingDropped, state.disjointDropped,
            state.invalidDropped, state.partialPackets);
        Write(drop);
        if (state.writer)
            state.writer->flush();
        Msg("* [gles_gpu_profile] raw_checkpoint frame=%llu ring_full_drops=%u disjoint_drops=%u invalid_drops=%u partial_packets=%u",
            static_cast<unsigned long long>(state.frame), state.pendingDropped, state.disjointDropped,
            state.invalidDropped, state.partialPackets);
        state.lastFlush = end;
    }
}

void RecordDraw()
{
    if (state.inFrame)
        ++state.draws;
}

void RecordVertexBinding(bool legacy)
{
    if (!state.inFrame)
        return;
    ++state.vertexBindings;
    state.legacyVertexBindings += legacy;
}

void RecordVisual(unsigned type)
{
    if (!state.inFrame || type >= state.mainVisuals.size())
        return;
    if (state.depth[unsigned(Stage::MainGeometry)])
        ++state.mainVisuals[type];
    if (state.depth[unsigned(Stage::LocalShadowGeometry)])
        ++state.shadowVisuals[type];
}

void RecordStaticBatch(unsigned draws, unsigned bytes)
{
    if (state.inFrame)
    {
        ++state.staticBatches;
        state.batchedDraws += draws;
        state.batchBytes += bytes;
    }
}

bool SamplingShadowAtlas()
{
    return state.inFrame && state.atlasOnly;
}

bool Capturing()
{
    return state.inFrame;
}

void RecordDetails(unsigned instances)
{
    if (!state.inFrame)
        return;
    state.detailInstances += instances;
    if (state.depth[unsigned(Stage::SunShadowAndDirect)])
        state.shadowDetailInstances += instances;
    if (state.depth[unsigned(Stage::LocalShadowDetails)])
        state.localDetails.instancesSubmitted += instances;
}

LocalDetailCullStats* SampleLocalDetailCull()
{
    if (state.inFrame && state.depth[unsigned(Stage::LocalShadowDetails)])
        return &state.localDetails;
    return nullptr;
}

Scope::Scope(Stage phase, bool gpuTimer) : stage(phase), event(InvalidEvent), start(0), threadStart(0), active(state.inFrame)
{
    if (state.atlasOnly)
        active = active && (stage == Stage::LocalShadowAtlas || stage == Stage::LocalTaskWait ||
            (stage == Stage::LocalShadowSetup && gpuTimer));
    if (!active)
        return;
    start = Now();
    threadStart = ThreadNow();
    ++state.depth[unsigned(stage)];
    ++state.invocations[unsigned(stage)];
    event = gpuTimer && !CpuOnlyLocalStage(stage) ? StartEvent(stage) : InvalidEvent;
}

Scope::~Scope() { Stop(); }

void Scope::Stop()
{
    if (!active)
        return;
    active = false;
    if (!state.inFrame)
        return;
    const auto end = Now();
    const auto cpu = ThreadNow();
    if (end >= start)
        state.wallNs[unsigned(stage)] += end - start;
    if (threadStart && cpu >= threadStart)
        state.threadNs[unsigned(stage)] += cpu - threadStart;
    else
        state.threadInvalid[unsigned(stage)] = true;
    --state.depth[unsigned(stage)];
    EndEvent(event);
}
} // namespace xray::render::RENDER_NAMESPACE::GLGpuProfiler
#endif
