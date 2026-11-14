#pragma once

#include <cstdint>

namespace AndroidCpuProfile
{
enum class Phase : unsigned
{
    FrameUpdate, FrameCallbacks, Level, Persistent, Objects, Scheduler, ALife,
    LevelEvents, LuaLevel, LuaGc, Physics, Parallel,
    LocalLights, LocalBuildMain, LocalBuildWorkers, LocalWait, LocalWaitWork, LocalSubmit,
    SchedulerRealtime, SchedulerNormal, SchedulerRegistration, SchedulerRealtimeWork, SchedulerNormalWork, LocalSort,
    LocalContextAlloc,
    StalkerTotal, StalkerObjectHandler, StalkerAnimation, StalkerAgent, StalkerVision, StalkerMemory,
    StalkerInherited, StalkerThink, StalkerBrain, StalkerMovement, StalkerTouch, StalkerInventory, StalkerPhysics,
    Count
};

#if defined(XR_PLATFORM_ANDROID) && !defined(_EDITOR)
struct Timing
{
    std::uint64_t wallNs = 0;
    std::uint64_t cpuNs = 0;
    unsigned calls = 0;
    unsigned cpuCalls = 0;
    bool running = false;
};

XRCORE_API void BeginFrame(bool eligible, const void* level);
XRCORE_API void MarkPresented();
XRCORE_API void FinishFrame(bool eligible, const void* level, unsigned frame);
XRCORE_API bool Sampling();
XRCORE_API Timing* Sample(Phase phase);
XRCORE_API Timing* SampleInside(Phase parent, Phase phase);
XRCORE_API Timing* WaitWorkSample();
XRCORE_API void Merge(Phase phase, const Timing& timing);
XRCORE_API void RecordScheduledUpdate(const char* type, bool realtime, const Timing& timing);
XRCORE_API void RecordStalkerUpdate(unsigned elapsedMs);

class XRCORE_API Timer
{
    Timing* timing = nullptr;
    std::uint64_t wallStart = 0;
    std::uint64_t cpuStart = 0;

public:
    explicit Timer(Timing* timing);
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    ~Timer();
};

class Scope final : public Timer
{
public:
    explicit Scope(Phase phase) : Timer(Sample(phase)) {}
};
#else
inline bool Sampling() { return false; }
inline void RecordStalkerUpdate(unsigned) {}

class Scope
{
public:
    explicit Scope(Phase) {}
};
#endif
}
