#pragma once
#include "glBackendStateCache.h"
#include "glStaticIndexBatchIndices.h"
#include "Layers/xrRender/FBasicVisual.h"
#include "Layers/xrRender/BufferUtils.h"
#include "xrEngine/AndroidSceneCompare.h"
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "glGpuProfiler.h"
#include <ctime>
#endif
#include <SDL.h>
#include <list>
#include <unordered_map>
#include <vector>

namespace xray::render::RENDER_NAMESPACE::GLStaticIndexBatch
{
constexpr size_t CapacityBytes = 64 * 1024 * 1024;
constexpr size_t SourceCapacityBytes = 32 * 1024 * 1024;
constexpr size_t MaxBatchIndices = 65536;
constexpr size_t MaxEntries = 4096;
constexpr u32 MaxUploadsPerFrame = 4;

inline bool MainEnabled()
{
    static const bool enabled = AndroidSceneCompare::HasOption(Core.Params, "-gles_static_main_batch") &&
        !AndroidSceneCompare::HasOption(Core.Params, "-gles_no_static_batch");
    return enabled;
}

inline bool ShadowEnabled()
{
    static const bool enabled = AndroidSceneCompare::HasOption(Core.Params, "-gles_static_shadow_batch") &&
        !AndroidSceneCompare::HasOption(Core.Params, "-gles_no_static_batch");
    return enabled;
}

struct Counters
{
    u32 hits{}, misses{}, uploads{}, rejected{}, budgetSkips{};
    u64 uploadBytes{}, lookupNs{}, convertNs{}, uploadNs{};
};

struct Stats
{
    Counters main, shadow;
    size_t residentBytes{}, residentEntries{}, retainedSourceBytes{};
};

struct MeshKey
{
    const IndexStagingBuffer* source;
    u32 base, offset, primitives, vertices;

    bool operator==(const MeshKey& other) const
    {
        return source == other.source && base == other.base && offset == other.offset &&
            primitives == other.primitives && vertices == other.vertices;
    }
};

inline MeshKey Key(const IRender_Mesh& mesh)
{
    return {mesh.p_rm_Indices, mesh.vBase, mesh.iBase, mesh.dwPrimitives, mesh.vCount};
}

struct Entry
{
    size_t hash;
    std::vector<MeshKey> meshes;
    GLuint buffer;
    size_t bytes;
    u32 lastFrame;
};

struct State
{
    SDL_GLContext context = nullptr;
    Stats counters{};
    size_t used = 0;
    bool failed = false;
    bool validFrame = false;
    u32 frame = 0, uploads = 0, evicted = 0;
    std::vector<u32> scratch;
    // The least-recently used entry is first. Cache hits never scan the list.
    std::list<Entry> entries;
    std::unordered_multimap<size_t, std::list<Entry>::iterator> cache;
};

inline State& Get()
{
    static State state;
    return state;
}

inline void Shutdown()
{
    auto& state = Get();
    if (state.context && SDL_GL_GetCurrentContext() == state.context)
    {
        for (const auto& entry : state.entries)
            glDeleteBuffers(1, &entry.buffer);
        if (!state.entries.empty())
            GLHardwareState::InvalidateGeometry();
    }
    state = {};
}

inline bool RetainSource(size_t bytes)
{
    auto& retained = Get().counters.retainedSourceBytes;
    if (bytes > SourceCapacityBytes - retained)
        return false;
    retained += bytes;
    return true;
}

inline Stats FrameStats(u32 frame)
{
    const auto& state = Get();
    Stats result = state.counters;
    if (!state.validFrame || state.frame != frame)
        result.main = result.shadow = {};
    result.residentBytes = state.used;
    result.residentEntries = state.cache.size();
    return result;
}

inline bool Begin()
{
    auto& state = Get();
    if (!state.validFrame || state.frame != Device.dwFrame)
    {
        const auto current = SDL_GL_GetCurrentContext();
        if (!current || (state.context && state.context != current))
            return false;
        state.context = current;
        state.validFrame = true;
        state.frame = Device.dwFrame;
        state.uploads = state.evicted = 0;
        state.counters.main = state.counters.shadow = {};
    }
    return !state.failed;
}

inline Counters* SampleCounters(bool shadow)
{
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    if (GLGpuProfiler::Capturing())
        return shadow ? &Get().counters.shadow : &Get().counters.main;
#endif
    return nullptr;
}

class CpuScope
{
    u64* result;
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    u64 start;

    static u64 Now()
    {
        timespec stamp{};
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &stamp) != 0)
            return 0;
        return u64(stamp.tv_sec) * 1000000000ULL + u64(stamp.tv_nsec);
    }
#endif

public:
    explicit CpuScope(u64* result) : result(result)
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        , start(result ? Now() : 0)
#endif
    {}

    ~CpuScope()
    {
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
        if (start)
        {
            const u64 end = Now();
            if (end >= start)
                *result += end - start;
        }
#endif
    }
};

inline bool MakeRoom(State& state, size_t bytes)
{
    while (bytes > CapacityBytes - state.used || state.entries.size() >= MaxEntries)
    {
        auto victim = state.entries.begin();
        if (victim == state.entries.end() || state.frame - victim->lastFrame <= 60 ||
            state.evicted >= MaxUploadsPerFrame)
            return false;
        const auto range = state.cache.equal_range(victim->hash);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second != victim)
                continue;
            state.cache.erase(it);
            break;
        }
        state.used -= victim->bytes;
        glDeleteBuffers(1, &victim->buffer);
        GLHardwareState::InvalidateGeometry();
        state.entries.erase(victim);
        ++state.evicted;
    }
    return true;
}

inline GLuint Find(State& state, size_t hash, const IRender_Mesh* const* meshes, u32 count)
{
    const auto range = state.cache.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it)
    {
        auto entry = it->second;
        if (entry->meshes.size() != count)
            continue;
        bool match = true;
        for (u32 i = 0; match && i < count; ++i)
            match = entry->meshes[i] == Key(*meshes[i]);
        if (match)
        {
            entry->lastFrame = state.frame;
            state.entries.splice(state.entries.end(), state.entries, entry);
            return entry->buffer;
        }
    }
    return 0;
}

inline bool Convert(std::vector<u32>& data, const IRender_Mesh* const* meshes, u32 count)
{
    data.clear();
    for (u32 i = 0; i < count; ++i)
    {
        const auto& mesh = *meshes[i];
        const u32 indices = mesh.dwPrimitives * 3;
        const auto* source = mesh.p_rm_Indices->ReadOnlyIndices(mesh.iBase, indices);
        if (indices > MaxBatchIndices - data.size() ||
            !StaticBatchIndices::Append(data, source, indices, mesh.vBase, mesh.vCount))
            return false;
    }
    return true;
}

inline GLuint Buffer(const IRender_Mesh* const* meshes, u32 count, size_t indexCount, Counters* stats)
{
    auto& state = Get();
    if (state.failed)
        return 0;
    size_t hash = size_t(14695981039346656037ull);
    {
        CpuScope timing(stats ? &stats->lookupNs : nullptr);
        const auto mix = [&](size_t value) { hash = (hash ^ value) * size_t(1099511628211ull); };
        for (u32 i = 0; i < count; ++i)
        {
            const auto key = Key(*meshes[i]);
            mix(reinterpret_cast<size_t>(key.source));
            mix(key.base);
            mix(key.offset);
            mix(key.primitives);
            mix(key.vertices);
        }
        if (GLuint buffer = Find(state, hash, meshes, count))
        {
            if (stats)
                ++stats->hits;
            return buffer;
        }
    }
    if (stats)
        ++stats->misses;
    if (state.uploads >= MaxUploadsPerFrame)
    {
        if (stats)
            ++stats->budgetSkips;
        return 0;
    }

    // Count conversion attempts too, including invalid sources and failed
    // allocations, so the cache cannot cause unbounded warm-up work.
    ++state.uploads;
    {
        CpuScope timing(stats ? &stats->convertNs : nullptr);
        state.scratch.reserve(indexCount);
        if (!Convert(state.scratch, meshes, count))
        {
            if (stats)
                ++stats->rejected;
            return 0;
        }
    }

    CpuScope timing(stats ? &stats->uploadNs : nullptr);
    const size_t bytes = state.scratch.size() * sizeof(u32);
    if (!MakeRoom(state, bytes))
    {
        if (stats)
            ++stats->budgetSkips;
        return 0;
    }
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    if (!buffer)
        return 0;
    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
    glBufferData(GL_COPY_WRITE_BUFFER, bytes, state.scratch.data(), GL_STATIC_DRAW);
    // Validate allocation without consuming a pending error from another pass.
    GLint stored = 0;
    glGetBufferParameteriv(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &stored);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    if (stored != GLint(bytes))
    {
        glDeleteBuffers(1, &buffer);
        state.failed = true;
        Msg("! [gles_static_batch] allocation failed; retaining original draws");
        return 0;
    }

    Entry entry{hash, {}, buffer, bytes, state.frame};
    entry.meshes.reserve(count);
    for (u32 i = 0; i < count; ++i)
        entry.meshes.push_back(Key(*meshes[i]));
    state.entries.push_back(std::move(entry));
    state.cache.emplace(hash, std::prev(state.entries.end()));
    state.used += bytes;
    if (stats)
    {
        ++stats->uploads;
        stats->uploadBytes += bytes;
    }
    return buffer;
}
}
