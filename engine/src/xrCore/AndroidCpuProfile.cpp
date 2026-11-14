#include "stdafx.h"
#include "AndroidCpuProfile.h"

#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cxxabi.h>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace AndroidCpuProfile
{
namespace
{
constexpr unsigned SampleStride = 8;
constexpr auto PhaseCount = unsigned(Phase::Count);
constexpr const char* Names[] = {
    "frame_update", "frame_callbacks", "level", "persistent", "objects", "scheduler", "alife",
    "level_events", "lua_level", "lua_gc", "physics", "parallel",
    "local_lights", "local_build_main", "local_build_workers", "local_wait", "local_wait_work", "local_submit",
    "scheduler_rt", "scheduler_normal", "scheduler_registration", "scheduler_rt_work", "scheduler_normal_work",
    "local_sort", "local_context_alloc",
    "stalker_total", "stalker_object_handler", "stalker_animation", "stalker_agent", "stalker_vision", "stalker_memory",
    "stalker_inherited", "stalker_think", "stalker_brain", "stalker_movement", "stalker_touch", "stalker_inventory", "stalker_physics"
};
static_assert(sizeof(Names) / sizeof(Names[0]) == PhaseCount);

struct ScheduledType
{
    const char* name = nullptr;
    bool realtime = false;
    Timing time;
    std::uint64_t maxWallNs = 0;
};

struct ScheduledTypes
{
    std::array<ScheduledType, 128> types{};
    unsigned size = 0;
    std::array<ScheduledType, 2> overflow{{{"<overflow>", false, {}, 0}, {"<overflow>", true, {}, 0}}};
};

struct State
{
    const void* level = nullptr;
    std::uint64_t windowStart = 0;
    unsigned frameCount = 0;
    unsigned sampleCount = 0;
    unsigned sequence = 0;
    bool presented = false;
    bool sampled = false;
    unsigned stalkerUpdates = 0;
    std::uint64_t stalkerElapsedMs = 0;
    unsigned stalkerMaxElapsedMs = 0;
    std::array<Timing, PhaseCount> frame{};
    std::array<Timing, PhaseCount> window{};
    ScheduledTypes frameTypes;
    ScheduledTypes windowTypes;
};
thread_local State state;

bool SelectSample(unsigned sequence)
{
    // Select one position in each eight-frame block. Hashing the block index
    // avoids locking every sample to one phase of periodic object updates.
    // Use a local integer hash, never the gameplay random number generator.
    auto block = sequence / SampleStride;
    block ^= block >> 16;
    block *= 0x7feb352du;
    block ^= block >> 15;
    block *= 0x846ca68bu;
    block ^= block >> 16;
    return sequence % SampleStride == block % SampleStride;
}

bool Enabled()
{
    constexpr char flag[] = "-gles_cpu_split";
    if (!Core.Params)
        return false;
    for (const char* p = Core.Params; (p = std::strstr(p, flag)) != nullptr; ++p)
    {
        if ((p == Core.Params || std::strchr(" \t\r\n", p[-1])) &&
            std::strchr(" \t\r\n", p[sizeof(flag) - 1]))
            return true;
    }
    return false;
}

std::uint64_t Now(clockid_t clock)
{
    timespec time{};
    if (clock_gettime(clock, &time) != 0)
        return 0;
    return std::uint64_t(time.tv_sec) * 1000000000ULL + std::uint64_t(time.tv_nsec);
}

void Accumulate(Timing& total, const Timing& value)
{
    total.wallNs += value.wallNs;
    total.cpuNs += value.cpuNs;
    total.calls += value.calls;
    total.cpuCalls += value.cpuCalls;
}

void AccumulateScheduled(ScheduledTypes& total, const ScheduledType& value)
{
    ScheduledType* entry = nullptr;
    for (unsigned i = 0; i < total.size; ++i)
    {
        auto& candidate = total.types[i];
        if (candidate.realtime == value.realtime &&
            (candidate.name == value.name || std::strcmp(candidate.name, value.name) == 0))
        {
            entry = &candidate;
            break;
        }
    }
    if (!entry && total.size < total.types.size())
    {
        entry = &total.types[total.size++];
        *entry = {value.name, value.realtime, {}, 0};
    }
    if (!entry)
        entry = &total.overflow[value.realtime];
    Accumulate(entry->time, value.time);
    entry->maxWallNs = std::max(entry->maxWallNs, value.maxWallNs);
}

void PrintScheduledTypes(unsigned frame)
{
    const auto& window = state.windowTypes;
    std::array<const ScheduledType*, 130> sorted{};
    unsigned size = 0;
    for (unsigned i = 0; i < window.size; ++i)
        sorted[size++] = &window.types[i];
    for (const auto& overflow : window.overflow)
        if (overflow.time.calls)
            sorted[size++] = &overflow;

    for (const bool byCpu : {true, false})
    {
        const auto cost = [byCpu](const ScheduledType* type)
        {
            return byCpu ? type->time.cpuNs : type->time.wallNs;
        };
        const auto top = std::min(size, 8u);
        std::partial_sort(sorted.begin(), sorted.begin() + top, sorted.begin() + size,
            [&](const auto* a, const auto* b) { return cost(a) > cost(b); });
        for (unsigned i = 0; i < top; ++i)
        {
            const auto& item = *sorted[i];
            int status = 0;
            char* pretty = abi::__cxa_demangle(item.name, nullptr, nullptr, &status);
            const double ms = 1e-6 / state.sampleCount;
            Msg("* [gles_sched_type] frame=%u sampled=%u rank_by=%s rank=%u queue=%s type=\"%.200s\""
                " wall_ms=%.3f cpu_ms=%.3f calls=%u max_call_wall_ms=%.3f",
                frame, state.sampleCount, byCpu ? "cpu" : "wall", i + 1, item.realtime ? "rt" : "normal",
                pretty ? pretty : item.name, item.time.wallNs * ms,
                item.time.calls == item.time.cpuCalls ? item.time.cpuNs * ms : -1.0,
                item.time.calls, item.maxWallNs * 1e-6);
            std::free(pretty);
        }
    }
}
}

void BeginFrame(bool eligible, const void* level)
{
    if (!eligible || !Enabled())
    {
        if (state.windowStart)
            state = {};
        return;
    }
    if (state.level != level)
    {
        state = {};
        state.level = level;
    }
    if (!state.windowStart)
        state.windowStart = Now(CLOCK_MONOTONIC);
    state.presented = false;
    state.sampled = state.windowStart && SelectSample(state.sequence++);
    if (state.sampled)
    {
        state.frame = {};
        state.frameTypes.size = 0;
        for (auto& overflow : state.frameTypes.overflow)
        {
            overflow.time = {};
            overflow.maxWallNs = 0;
        }
    }
}

void MarkPresented()
{
    state.presented = true;
}

void FinishFrame(bool eligible, const void* level, unsigned frame)
{
    if (!state.windowStart)
        return;
    if (!eligible || !state.presented || state.level != level)
    {
        state = {};
        return;
    }
    if (state.sampled)
    {
        for (unsigned i = 0; i < PhaseCount; ++i)
            Accumulate(state.window[i], state.frame[i]);
        for (unsigned i = 0; i < state.frameTypes.size; ++i)
            AccumulateScheduled(state.windowTypes, state.frameTypes.types[i]);
        for (const auto& value : state.frameTypes.overflow)
        {
            auto& total = state.windowTypes.overflow[value.realtime];
            Accumulate(total.time, value.time);
            total.maxWallNs = std::max(total.maxWallNs, value.maxWallNs);
        }
        ++state.sampleCount;
    }
    state.sampled = false;
    ++state.frameCount;
    const auto end = Now(CLOCK_MONOTONIC);
    if (end < state.windowStart || end - state.windowStart < 5000000000ULL)
        return;
    if (state.sampleCount)
    {
        // The file writer has a smaller per-call formatting buffer than Msg.
        // Keep the AI breakdown on its own row, joined by device_frame/sampled.
        char line[1000]; // Leave room for the file sink's CR/LF and terminator.
        int used = std::snprintf(line, sizeof(line),
            "* [gles_cpu_split] schema=2 device_frame=%u frames=%u sampled=%u stride=%u sampling=hashed_block elapsed_ms=%.3f"
            " metrics=wall_ms/cpu_ms/calls_per_sample",
            frame, state.frameCount, state.sampleCount, SampleStride, double(end - state.windowStart) * 1e-6);
        const double ms = 1e-6 / state.sampleCount;
        for (unsigned i = 0; i < unsigned(Phase::StalkerTotal) && used >= 0 && size_t(used) < sizeof(line); ++i)
        {
            const auto& value = state.window[i];
            used += std::snprintf(line + used, sizeof(line) - size_t(used), " %s=%.3f/%.3f/%.2f",
                Names[i], double(value.wallNs) * ms,
                value.calls == value.cpuCalls ? double(value.cpuNs) * ms : -1.0,
                double(value.calls) / state.sampleCount);
        }
        Msg("%s", line);
        used = std::snprintf(line, sizeof(line),
            "* [gles_cpu_stalker] schema=2 device_frame=%u frames=%u sampled=%u stride=%u sampling=hashed_block elapsed_ms=%.3f"
            " stalker_updates=%u stalker_dt_sum_ms=%llu stalker_dt_max_ms=%u metrics=wall_ms/cpu_ms/calls_per_sample",
            frame, state.frameCount, state.sampleCount, SampleStride, double(end - state.windowStart) * 1e-6,
            state.stalkerUpdates, static_cast<unsigned long long>(state.stalkerElapsedMs), state.stalkerMaxElapsedMs);
        for (unsigned i = unsigned(Phase::StalkerTotal); i < PhaseCount && used >= 0 && size_t(used) < sizeof(line); ++i)
        {
            const auto& value = state.window[i];
            used += std::snprintf(line + used, sizeof(line) - size_t(used), " %s=%.3f/%.3f/%.2f",
                Names[i], double(value.wallNs) * ms,
                value.calls == value.cpuCalls ? double(value.cpuNs) * ms : -1.0,
                double(value.calls) / state.sampleCount);
        }
        Msg("%s", line);
        PrintScheduledTypes(frame);
    }
    state.windowStart = end;
    state.window = {};
    state.windowTypes = {};
    state.frameCount = 0;
    state.sampleCount = 0;
    state.stalkerUpdates = 0;
    state.stalkerElapsedMs = 0;
    state.stalkerMaxElapsedMs = 0;
}

bool Sampling()
{
    return state.sampled;
}

Timing* Sample(Phase phase)
{
    return state.sampled ? &state.frame[unsigned(phase)] : nullptr;
}

Timing* WaitWorkSample()
{
    return SampleInside(Phase::LocalWait, Phase::LocalWaitWork);
}

Timing* SampleInside(Phase parent, Phase phase)
{
    if (state.sampled && state.frame[unsigned(parent)].running)
        return &state.frame[unsigned(phase)];
    return nullptr;
}

void Merge(Phase phase, const Timing& timing)
{
    if (auto* total = Sample(phase))
        Accumulate(*total, timing);
}

void RecordScheduledUpdate(const char* type, bool realtime, const Timing& timing)
{
    if (!state.sampled)
        return;
    Merge(realtime ? Phase::SchedulerRealtimeWork : Phase::SchedulerNormalWork, timing);
    AccumulateScheduled(state.frameTypes, {type, realtime, timing, timing.wallNs});
}

void RecordStalkerUpdate(unsigned elapsedMs)
{
    // Count every callback in an active profiling window, including frames
    // that are not timed. This reports cadence without changing the scheduler.
    if (!state.windowStart)
        return;
    ++state.stalkerUpdates;
    state.stalkerElapsedMs += elapsedMs;
    state.stalkerMaxElapsedMs = std::max(state.stalkerMaxElapsedMs, elapsedMs);
}

Timer::Timer(Timing* value)
{
    if (!value || value->running)
        return;
    timing = value;
    timing->running = true;
    wallStart = Now(CLOCK_MONOTONIC);
    cpuStart = Now(CLOCK_THREAD_CPUTIME_ID);
}

Timer::~Timer()
{
    if (!timing)
        return;
    const auto cpuEnd = Now(CLOCK_THREAD_CPUTIME_ID);
    const auto wallEnd = Now(CLOCK_MONOTONIC);
    ++timing->calls;
    if (wallStart && wallEnd >= wallStart)
        timing->wallNs += wallEnd - wallStart;
    if (cpuStart && cpuEnd >= cpuStart)
    {
        timing->cpuNs += cpuEnd - cpuStart;
        ++timing->cpuCalls;
    }
    timing->running = false;
}
}
#endif
