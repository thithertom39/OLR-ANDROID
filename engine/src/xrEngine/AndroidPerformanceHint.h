#pragma once

// Experimental opt-in, used only by device.cpp on the engine's main thread.
// No Java, application-wide policy, worker affinity or gameplay scheduling.
#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>

// The native API starts at Android 33. Opaque types + runtime lookup keep the
// minSdk 24 binary free of mandatory newer-API symbol imports.
struct APerformanceHintManager;
struct APerformanceHintSession;

namespace AndroidPerformanceHint
{
constexpr std::int64_t TargetNs = 16666667;
using GetManager = APerformanceHintManager* (*)();
using CreateSession = APerformanceHintSession* (*)(APerformanceHintManager*, const std::int32_t*, size_t, std::int64_t);
using PreferredRate = std::int64_t (*)(APerformanceHintManager*);
using ReportDuration = int (*)(APerformanceHintSession*, std::int64_t);
using CloseSession = void (*)(APerformanceHintSession*);

struct State
{
    bool attempted = false;
    bool unavailable = false;
    void* library = nullptr; // Retained until process exit; never unload live function pointers.
    APerformanceHintManager* manager = nullptr;
    APerformanceHintSession* session = nullptr;
    CreateSession create = nullptr;
    ReportDuration report = nullptr;
    CloseSession close = nullptr;
    const void* level = nullptr;
    std::uint64_t start = 0;
    std::uint64_t presentStart = 0;
    std::uint64_t presentNs = 0;
    bool presented = false;
    unsigned reports = 0;
};

inline State state;

inline bool Enabled()
{
    constexpr char flag[] = "-android_perf_hint";
    if (!Core.Params)
        return false;
    for (const char* p = Core.Params; (p = std::strstr(p, flag)) != nullptr; ++p)
        if ((p == Core.Params || std::strchr(" \t\r\n", p[-1])) &&
            std::strchr(" \t\r\n", p[sizeof(flag) - 1]))
            return true;
    return false;
}

inline std::uint64_t Now()
{
    timespec value{};
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
        return 0;
    return std::uint64_t(value.tv_sec) * 1000000000ULL + std::uint64_t(value.tv_nsec);
}

inline void ClearFrame()
{
    state.start = state.presentStart = state.presentNs = 0;
    state.presented = false;
}

// Every call, including close, must run on the same engine main thread.
// Focus loss uses OnWindowActivate before that thread can stop producing frames.
inline void Stop()
{
    ClearFrame();
    if (state.session)
    {
        state.close(state.session);
        state.session = nullptr;
        Msg("* [android_perf_hint] closed reports=%u", state.reports);
    }
    state.level = nullptr;
    state.reports = 0;
}

inline bool Initialize()
{
    if (state.attempted)
        return !state.unavailable;
    state.attempted = true;
    state.unavailable = true;
    state.library = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (!state.library)
    {
        Msg("* [android_perf_hint] unsupported: libandroid unavailable");
        return false;
    }
    const auto get = reinterpret_cast<GetManager>(dlsym(state.library, "APerformanceHint_getManager"));
    const auto preferred = reinterpret_cast<PreferredRate>(dlsym(state.library, "APerformanceHint_getPreferredUpdateRateNanos"));
    state.create = reinterpret_cast<CreateSession>(dlsym(state.library, "APerformanceHint_createSession"));
    state.report = reinterpret_cast<ReportDuration>(dlsym(state.library, "APerformanceHint_reportActualWorkDuration"));
    state.close = reinterpret_cast<CloseSession>(dlsym(state.library, "APerformanceHint_closeSession"));
    if (!get || !preferred || !state.create || !state.report || !state.close || !(state.manager = get()))
    {
        Msg("* [android_perf_hint] unsupported: native API or manager unavailable");
        return false;
    }
    Msg("* [android_perf_hint] native API available preferred_update_ns=%lld",
        static_cast<long long>(preferred(state.manager)));
    state.unavailable = false;
    return true;
}

inline void BeginFrame(bool eligible, const void* level)
{
    if (!eligible || !level)
    {
        if (state.session)
            Stop();
        return;
    }
    static const bool enabled = Enabled();
    if (!enabled)
        return;
    if (!Initialize())
        return;
    if (state.level != level)
        Stop();
    if (!state.session)
    {
        const std::int32_t tid = gettid();
        state.session = state.create(state.manager, &tid, 1, TargetNs);
        if (!state.session)
        {
            state.unavailable = true; // One failed attempt, never retry each frame.
            Msg("* [android_perf_hint] unsupported: session creation failed tid=%d", tid);
            return;
        }
        state.level = level;
        Msg("* [android_perf_hint] created tid=%d target_ns=%lld threads=1 mode=main_work_excluding_present",
            tid, static_cast<long long>(TargetNs));
    }
    ClearFrame();
    state.start = Now();
}

inline void BeforePresent()
{
    if (state.start)
    {
        if (state.presentStart || state.presented)
            ClearFrame(); // Reject unexpected duplicate/nested presentation.
        else
            state.presentStart = Now();
    }
}

inline void AfterPresent()
{
    if (!state.start || !state.presentStart)
        return;
    const auto end = Now();
    if (end < state.presentStart)
    {
        ClearFrame();
        return;
    }
    state.presentNs = end - state.presentStart;
    state.presentStart = 0;
    state.presented = true;
}

inline void FinishFrame(bool eligible, const void* level)
{
    if (!state.session)
        return;
    if (!eligible || state.level != level)
    {
        Stop();
        return;
    }
    if (!state.start || state.presentStart || !state.presented)
    {
        ClearFrame();
        return;
    }
    const auto end = Now();
    const auto start = state.start;
    const auto excluded = state.presentNs;
    ClearFrame(); // A second FinishFrame cannot report the same work twice.
    if (end <= start || excluded >= end - start)
        return;
    const auto actual = static_cast<std::int64_t>(end - start - excluded);
    const int error = state.report(state.session, actual);
    if (error)
    {
        Msg("* [android_perf_hint] report failed error=%d; disabled", error);
        Stop();
        state.unavailable = true;
        return;
    }
    if (++state.reports == 1)
        Msg("* [android_perf_hint] first report accepted actual_ns=%lld", static_cast<long long>(actual));
}
}
#endif
