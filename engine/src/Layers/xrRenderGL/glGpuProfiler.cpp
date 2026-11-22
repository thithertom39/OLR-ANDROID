#include "stdafx.h"
#include "glGpuProfiler.h"

#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "xrEngine/IGame_Persistent.h"
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
constexpr const char* Names[StageCount] = {
    "render_total", "world", "main_geometry", "details_all_submissions",
    "sun_shadow_and_direct", "local_lights", "combine", "forward_world",
    "bloom_luminance_inclusive", "luminance", "final_combine_aa", "extra_pp", "present_blit",
    "z_prefill", "sync_wait", "scene_prepare", "light_visibility", "wallmarks_hud_ui",
    "rain", "emissive", "light_flush", "calculate", "hom_wait",
    "local_normal", "local_pending", "local_visibility_cpu", "local_pack_cpu", "local_task_wait_cpu", "local_shadow_setup", "local_shadow_geometry", "local_shadow_details", "local_shadow_special", "local_smapvis_end", "local_point_accum", "local_spot_accum", "local_indirect", "local_volumetric", "sun_shadow_geometry", "sun_details", "sun_special", "sun_accum", "local_shadow_atlas",
    "r1_forward_world", "r1_portal_traversal_cpu", "r1_lod", "r1_detail_instances"
};

struct Event
{
    Stage stage{};
    bool ended = false;
};
struct Packet
{
    std::array<Event, EventCapacity> events{};
    unsigned count = 0;
    unsigned lastQuery = 0;
    unsigned epoch = 0;
    std::uint64_t frame = 0;
    unsigned deviceFrame = 0;
    bool pending = false;
    bool overflow = false;
};
struct State
{
    bool enabled = false;
    bool atlasOnly = false;
    bool gpu = false;
    bool inFrame = false;
    bool gpuEnded = false;
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
    std::uint64_t frame = 0;
    unsigned deviceFrame = 0;
    std::uint64_t start = 0;
    std::uint64_t threadStart = 0;
    std::uint64_t previousStart = 0;
    std::uint64_t lastFlush = 0;
    double intervalMs = -1;
    unsigned draws = 0;
    unsigned triangles = 0, framebufferBinds = 0;
    std::array<unsigned,4> r1Draws{};
    xr_map<xr_string,unsigned> r1Materials;
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

// Result availability is polled only for an old packet, never in a wait loop.
void Poll()
{
    if (!state.gpu)
        return;
    if (!state.atlasOnly)
        DropDisjoint();
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
    if (state.atlasOnly)
    {
        DropDisjoint();
        if (!packet.pending)
            return;
    }
    GLuint available = GL_FALSE;
    glGetQueryObjectuivEXT(state.queries[oldest * state.queryStride + packet.lastQuery],
        GL_QUERY_RESULT_AVAILABLE_EXT, &available);
    if (!available)
        return;

    std::array<double, StageCount> milliseconds{};
    bool valid = packet.epoch == state.epoch && !packet.overflow;
    for (unsigned i = 0; i < packet.count; ++i)
    {
        const auto& event = packet.events[i];
        if (!event.ended)
        {
            valid = false;
            continue;
        }
        GLuint64 begin = 0, end = 0;
        glGetQueryObjectui64vEXT(state.queries[oldest * state.queryStride + i * 2], GL_QUERY_RESULT_EXT, &begin);
        glGetQueryObjectui64vEXT(state.queries[oldest * state.queryStride + i * 2 + 1], GL_QUERY_RESULT_EXT, &end);
        // Drop wrap or impossible intervals rather than manufacturing timings.
        if (end < begin || end - begin > 1000000000ULL)
            valid = false;
        else
            milliseconds[unsigned(event.stage)] += double(end - begin) * 1e-6;
    }
    DropDisjoint();
    valid = valid && packet.pending && packet.epoch == state.epoch;
    if (valid)
    {
        char row[8192];
        int used = std::snprintf(row, sizeof(row), "{\"record\":\"gpu\",\"frame\":%llu,\"device_frame\":%u,\"epoch\":%u,\"events\":%u,\"inclusive_ms\":[",
            static_cast<unsigned long long>(packet.frame), packet.deviceFrame, packet.epoch, packet.count);
        for (unsigned i = 0; i < StageCount; ++i)
        {
            const auto stage = Stage(i);
            const bool cpuOnly = (stage == Stage::LocalShadowSetup && !state.atlasOnly) ||
                stage == Stage::LocalSmapvisEnd || stage == Stage::LocalIndirect || stage == Stage::Details ||
                stage == Stage::LocalVisibility || stage == Stage::LocalPack || stage == Stage::LocalTaskWait ||
                stage == Stage::R1Traversal;
            used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%.6f", i ? "," : "",
                cpuOnly ? -1.0 : milliseconds[i]);
        }
        std::snprintf(row + used, sizeof(row) - size_t(used), "]}\n");
        Write(row);
    }
    else if (packet.pending)
        ++state.invalidDropped;
    packet.pending = false;
}

unsigned StartEvent(Stage stage)
{
    if (state.slot < 0 || state.gpuEnded)
        return InvalidEvent;
    auto& packet = state.packets[unsigned(state.slot)];
    if (packet.count == state.queryStride / 2)
    {
        packet.overflow = true;
        return InvalidEvent;
    }
    const unsigned event = packet.count++;
    packet.events[event] = {stage, false};
    glQueryCounterEXT(state.queries[unsigned(state.slot) * state.queryStride + event * 2], GL_TIMESTAMP_EXT);
    return event;
}

void EndEvent(unsigned event)
{
    if (event == InvalidEvent || state.slot < 0)
        return;
    auto& packet = state.packets[unsigned(state.slot)];
    if (event >= packet.count)
        return;
    const unsigned query = event * 2 + 1;
    glQueryCounterEXT(state.queries[unsigned(state.slot) * state.queryStride + query], GL_TIMESTAMP_EXT);
    packet.events[event].ended = true;
    packet.lastQuery = query;
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
        glDeleteQueriesEXT(GLsizei(RingSize * state.queryStride), state.queries.data());
    state = State{};
}

void Initialize()
{
    // The previous context may already be gone. Never delete its names in a
    // newly created context; normal shutdown/reset deletes while current.
    Shutdown(false);
    state.atlasOnly = Requested(Core.Params, "-gles_shadow_atlas_profile");
    state.enabled = state.atlasOnly || Requested(Core.Params, "-gles_gpu_profile");
    if (!state.enabled)
        return;
    if (state.atlasOnly)
        state.queryStride = 32;
    const unsigned stride = state.atlasOnly ? AtlasSampleStride : SampleStride;
    state.gpu = GLAD_GL_EXT_disjoint_timer_query && glGenQueriesEXT && glDeleteQueriesEXT &&
        glQueryCounterEXT && glGetQueryObjectuivEXT && glGetQueryObjectui64vEXT && glGetQueryivEXT;
    if (state.gpu)
    {
        glGetQueryivEXT(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, &state.bits);
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
    Msg("* [gles_gpu_profile] diagnostic=on gpu_timestamps=%u counter_bits=%d sample_stride=%u event_capacity=%u file=%s gameplay_only=1 cpu_and_gpu_sampled=1 buffered_flush=5s",
        unsigned(state.gpu), state.bits, stride, state.queryStride / 2, state.writer ? name : "unavailable");
    char header[8192];
    int used = std::snprintf(header, sizeof(header), "{\"record\":\"metadata\",\"schema\":3,\"gameplay_only\":true,\"cpu_and_gpu_sampled\":true,\"timing\":\"wall includes waits; thread_cpu excludes descheduling; gpu timestamps exclude swap; nested stages overlap\",\"gpu_available\":%s,\"counter_bits\":%d,\"sample_stride\":%u,\"event_capacity\":%u,\"frame_id\":\"process_render_sequence\",\"stages\":[",
        state.gpu ? "true" : "false", state.bits, stride, state.queryStride / 2);
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
    state.triangles = state.framebufferBinds = 0;
    state.r1Draws = {};
    state.r1Materials.clear();
    state.localDetails = {};
    state.wallNs = {};
    state.threadNs = {};
    state.threadInvalid = {};
    state.invocations = {};
    state.invocations[unsigned(Stage::RenderTotal)] = 1;
    state.depth = {};
    if (!state.gpu)
        return;
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
    if (!state.atlasOnly)
        state.totalEvent = StartEvent(Stage::RenderTotal);
}

void EndGpuFrame()
{
    if (!state.inFrame || state.gpuEnded)
        return;
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
        "{\"record\":\"cpu\",\"frame\":%llu,\"device_frame\":%u,\"width\":%u,\"height\":%u,\"camera\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],\"interval_ms\":%.6f,\"render_wall_ms\":%.6f,\"render_thread_cpu_ms\":%.6f,\"draw_calls\":%u,\"detail_instances_submitted\":%u,\"sun_detail_instances_submitted\":%u,\"inclusive_wall_ms\":[",
        static_cast<unsigned long long>(state.frame), state.deviceFrame, Device.dwWidth, Device.dwHeight,
        double(Device.vCameraPosition.x), double(Device.vCameraPosition.y), double(Device.vCameraPosition.z),
        double(Device.vCameraDirection.x), double(Device.vCameraDirection.y), double(Device.vCameraDirection.z),
        state.intervalMs, wallMs, cpuMs, state.draws, state.detailInstances, state.shadowDetailInstances);
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%.6f", i ? "," : "", double(state.wallNs[i]) * 1e-6);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), "],\"inclusive_thread_cpu_ms\":[");
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%.6f", i ? "," : "",
            state.threadInvalid[i] ? -1.0 : double(state.threadNs[i]) * 1e-6);
    used += std::snprintf(row + used, sizeof(row) - size_t(used), "],\"logical_stage_invocations\":[");
    for (unsigned i = 0; i < StageCount; ++i)
        used += std::snprintf(row + used, sizeof(row) - size_t(used), "%s%u", i ? "," : "", state.invocations[i]);
    const auto& details = state.localDetails;
    std::snprintf(row + used, sizeof(row) - size_t(used),
        "],\"local_detail_cull\":{\"model_tests\":%u,\"models_rejected\":%u,"
        "\"model_bounds_built\":%u,\"cell_bounds_built\":%u,\"cells_tested\":%u,\"cells_rejected\":%u,"
        "\"instance_candidates\":%u,\"instances_skipped_by_cells\":%u,"
        "\"instance_tests\":%u,\"local_instances_submitted\":%u}}\n",
        details.modelTests, details.modelsRejected, details.modelBoundsBuilt, details.cellBoundsBuilt,
        details.cellsTested, details.cellsRejected, details.instanceCandidates, details.instancesSkipped,
        details.instanceCandidates - details.instancesSkipped, details.instancesSubmitted);
    Write(row);
    std::snprintf(row, sizeof(row),
        "{\"record\":\"r1_submissions\",\"frame\":%llu,\"triangles\":%u,\"renderer_framebuffer_binds\":%u,"
        "\"fallback_draws\":%u,\"native_base_draws\":%u,\"native_point_draws\":%u,\"native_spot_draws\":%u}\n",
        static_cast<unsigned long long>(state.frame),state.triangles,state.framebufferBinds,
        state.r1Draws[0],state.r1Draws[1],state.r1Draws[2],state.r1Draws[3]);
    Write(row);
    for (const auto& material : state.r1Materials)
    {
        std::snprintf(row,sizeof(row),"{\"record\":\"r1_material\",\"frame\":%llu,\"shader\":\"%s\",\"draws\":%u}\n",
            static_cast<unsigned long long>(state.frame),material.first.c_str(),material.second);
        Write(row);
    }
    if (end - state.lastFlush >= 5000000000ULL)
    {
        if (state.writer)
            state.writer->flush();
        Msg("* [gles_gpu_profile] raw_checkpoint frame=%llu ring_full_drops=%u disjoint_drops=%u invalid_drops=%u",
            static_cast<unsigned long long>(state.frame), state.pendingDropped, state.disjointDropped, state.invalidDropped);
        state.lastFlush = end;
    }
}

void RecordDraw(unsigned triangles)
{
    if (state.inFrame)
    {
        ++state.draws;
        state.triangles += triangles;
    }
}

void RecordFramebuffer()
{
    if (state.inFrame) ++state.framebufferBinds;
}

void RecordR1Material(const char* shader, unsigned draws, unsigned kind)
{
    if (!state.inFrame || !draws || kind >= state.r1Draws.size()) return;
    state.r1Draws[kind] += draws;
    xr_string name = kind == 0 ? "fallback/" : "native/";
    name += shader ? shader : "unknown";
    // Resource names are diagnostic labels, not arbitrary JSON fragments.
    for (auto& c : name) if (c == '\\' || c == '"' || c < ' ') c = '/';
    state.r1Materials[name] += draws;
}

const char* MaterialName(const SPass* pass)
{
    // Depth-only/vertex-only passes legitimately have no pixel shader.
    if (!pass) return "unknown";
    if (pass->ps) return pass->ps->cName.c_str();
    if (pass->vs) return pass->vs->cName.c_str();
    return "fixed_state";
}

bool SamplingShadowAtlas()
{
    return state.inFrame && state.atlasOnly;
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
    event = gpuTimer ? StartEvent(stage) : InvalidEvent;
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
