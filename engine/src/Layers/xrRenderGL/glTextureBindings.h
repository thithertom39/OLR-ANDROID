#pragma once

#include <array>
#include "glBackendStateCache.h"

// The command lists share a physical GL context. Cache the driver bindings,
// rather than the CTexture wrappers whose surfaces may change independently.
namespace xray::render::RENDER_NAMESPACE::GLTextureBindings
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
namespace detail
{
constexpr u32 Capacity = CTexture::mtMaxCombinedShaderTextures;
static_assert(Capacity <= 64);
constexpr size_t TargetCount = 6;
constexpr GLenum Targets[TargetCount] = {
    GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP,
    GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_CUBE_MAP_ARRAY
};

struct Binding
{
    GLuint name{};
    bool valid{};
};

struct Cache
{
    std::array<std::array<Binding, TargetCount>, Capacity> textures{};
    std::array<Binding, Capacity> samplers{};
    // Proof for the targets visited by ClearStage, not for untracked GL targets.
    // Unknown/reset state must first take the full baseline traversal.
    u64 stagesToClear = ~u64{0};
    // Unknown bindings must be visited after every physical-cache reset.
    // Once known, only the nonzero prefix can need clearing by another pass.
    u32 samplerExtent = Capacity;
    u32 activeUnit{};
    bool activeValid{};
    GLint maxUnits{};
};

inline Cache& State()
{
    static thread_local Cache cache;
    return cache;
}

inline GLint MaxUnits()
{
    auto& cache = State();
    if (cache.maxUnits <= 0)
        CHK_GL(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &cache.maxUnits));
    return cache.maxUnits;
}

inline bool UnitSupported(u32 unit)
{
    const GLint maxUnits = MaxUnits();
    return maxUnits > 0 && unit < static_cast<u32>(maxUnits);
}

inline void TrimSamplerExtent()
{
    auto& cache = State();
    while (cache.samplerExtent)
    {
        const u32 unit = cache.samplerExtent - 1;
        const auto& binding = cache.samplers[unit];
        // Units beyond the context limit cannot have physical bindings.
        const bool unsupported = cache.maxUnits > 0 && unit >= static_cast<u32>(cache.maxUnits);
        if (!unsupported && (!binding.valid || binding.name != 0))
            break;
        --cache.samplerExtent;
    }
}

inline bool TargetSupported(size_t index)
{
    if (index == 3)
        return GLAD_GL_ES_VERSION_3_1 || GLAD_GL_ES_VERSION_3_2;
    if (index == 5)
        return GLAD_GL_ES_VERSION_3_2 || GLAD_GL_EXT_texture_cube_map_array || GLAD_GL_OES_texture_cube_map_array;
    return true;
}

inline size_t TargetIndex(GLenum target)
{
    for (size_t index = 0; index < TargetCount; ++index)
        if (Targets[index] == target)
            return index;
    return TargetCount;
}

inline u32 ActiveUnit()
{
    auto& cache = State();
    if (!cache.activeValid)
    {
        GLint active = GL_TEXTURE0;
        CHK_GL(glGetIntegerv(GL_ACTIVE_TEXTURE, &active));
        cache.activeUnit = static_cast<u32>(active - GL_TEXTURE0);
        cache.activeValid = true;
    }
    return cache.activeUnit;
}
} // namespace detail

inline void Invalidate()
{
    detail::State() = detail::Cache{};
}

inline u64 StagesToClear()
{
    return detail::State().stagesToClear;
}

inline void Activate(u32 unit, bool force = false)
{
    auto& cache = detail::State();
    if (!detail::UnitSupported(unit))
    {
        // Remember an invalid requested unit so a subsequent Bind cannot
        // accidentally modify the previously active, valid unit.
        cache.activeUnit = unit;
        cache.activeValid = true;
        return;
    }
    if (force || !cache.activeValid || cache.activeUnit != unit)
        CHK_GL(glActiveTexture(GL_TEXTURE0 + unit));
    cache.activeUnit = unit;
    cache.activeValid = true;
}

inline void Bind(GLenum target, GLuint texture, bool force = false)
{
    auto& cache = detail::State();
    const u32 unit = detail::ActiveUnit();
    if (!detail::UnitSupported(unit))
        return;
    const size_t index = detail::TargetIndex(target);
    if (index == detail::TargetCount || unit >= detail::Capacity)
    {
        if (unit < detail::Capacity)
            cache.stagesToClear |= u64{1} << unit;
        if (force)
            Activate(unit, true);
        CHK_GL(glBindTexture(target, texture));
        return;
    }
    if (!detail::TargetSupported(index))
    {
        VERIFY(texture == 0);
        return;
    }
    auto& binding = cache.textures[unit][index];
    if (force || !binding.valid || binding.name != texture)
    {
        cache.stagesToClear |= u64{1} << unit;
        if (force)
            Activate(unit, true);
        CHK_GL(glBindTexture(target, texture));
        binding = { texture, true };
    }
}

// A draw binding does not need to change GL_ACTIVE_TEXTURE when the desired
// object is already bound on this unit. Upload/parameter callers still use
// Activate + Bind because their following glTex* calls depend on the active unit.
inline void BindAtUnit(u32 unit, GLenum target, GLuint texture, bool force = false)
{
    if (!detail::UnitSupported(unit))
        return;
    const size_t index = detail::TargetIndex(target);
    if (!force && unit < detail::Capacity && index < detail::TargetCount)
    {
        const auto& binding = detail::State().textures[unit][index];
        if (binding.valid && binding.name == texture)
            return;
    }
    Activate(unit, force);
    Bind(target, texture, force);
}

inline void ClearStage(u32 unit, bool force = false)
{
    if (!detail::UnitSupported(unit))
        return;
    auto& cache = detail::State();
    const u64 stageBit = unit < detail::Capacity ? u64{1} << unit : 0;
    // The original traversal would skip every supported target and leave the
    // active unit unchanged. Keep force and out-of-cache requests on that path.
    if (!force && stageBit && !(cache.stagesToClear & stageBit))
        return;
    for (size_t index = 0; index < detail::TargetCount; ++index)
    {
        if (!detail::TargetSupported(index))
            continue;
        if (!force && unit < detail::Capacity)
        {
            const auto& binding = cache.textures[unit][index];
            if (binding.valid && binding.name == 0)
                continue;
        }
        Activate(unit, force);
        Bind(detail::Targets[index], 0, force);
    }
    cache.stagesToClear &= ~stageBit;
}

// Include both the pass's used prefix and any unknown/nonzero hardware tail.
// A command-list-local pass pointer cannot prove another list left GL clear.
inline u32 SamplerStageCount(u32 usedStageCount, bool force = false)
{
    if (force)
        return detail::Capacity;
    const u32 desired = usedStageCount < detail::Capacity ? usedStageCount : detail::Capacity;
    const u32 previous = detail::State().samplerExtent;
    return desired > previous ? desired : previous;
}

inline void BindSampler(u32 unit, GLuint sampler, bool force = false)
{
    if (!detail::UnitSupported(unit))
    {
        VERIFY(sampler == 0);
        detail::TrimSamplerExtent();
        return;
    }
    auto& cache = detail::State();
    if (unit >= detail::Capacity)
    {
        CHK_GL(glBindSampler(unit, sampler));
        return;
    }
    auto& binding = cache.samplers[unit];
    if (force || !binding.valid || binding.name != sampler)
    {
        CHK_GL(glBindSampler(unit, sampler));
        binding = { sampler, true };
        if (sampler != 0 && cache.samplerExtent <= unit)
            cache.samplerExtent = unit + 1;
        else if (sampler == 0 && cache.samplerExtent == unit + 1)
            detail::TrimSamplerExtent();
    }
}

inline void DeleteTextures(GLsizei count, const GLuint* textures)
{
    CHK_GL(glDeleteTextures(count, textures));
    auto& cache = detail::State();
    bool deleted = false;
    for (GLsizei index = 0; index < count; ++index)
    {
        if (textures[index] == 0)
            continue;
        deleted = true;
        for (auto& unit : cache.textures)
            for (auto& binding : unit)
                if (binding.valid && binding.name == textures[index])
                    binding = { 0, true };
    }
    if (deleted)
        GLHardwareState::InvalidateAttachments();
}

inline void DeleteSamplers(GLsizei count, const GLuint* samplers)
{
    CHK_GL(glDeleteSamplers(count, samplers));
    auto& cache = detail::State();
    for (GLsizei index = 0; index < count; ++index)
    {
        if (samplers[index] == 0)
            continue;
        for (auto& binding : cache.samplers)
            if (binding.valid && binding.name == samplers[index])
                binding = { 0, true };
    }
    detail::TrimSamplerExtent();
}
#else
inline void Invalidate() {}
inline void Activate(u32 unit, bool = false) { CHK_GL(glActiveTexture(GL_TEXTURE0 + unit)); }
inline void Bind(GLenum target, GLuint texture, bool = false) { CHK_GL(glBindTexture(target, texture)); }
inline void BindAtUnit(u32 unit, GLenum target, GLuint texture, bool force = false)
{
    Activate(unit, force);
    Bind(target, texture, force);
}
inline void ClearStage(u32 unit, bool = false)
{
    Activate(unit);
    Bind(GL_TEXTURE_2D, 0);
    if (RImplementation.o.msaa)
        Bind(GL_TEXTURE_2D_MULTISAMPLE, 0);
    Bind(GL_TEXTURE_3D, 0);
    Bind(GL_TEXTURE_CUBE_MAP, 0);
}
inline void BindSampler(u32 unit, GLuint sampler, bool = false) { CHK_GL(glBindSampler(unit, sampler)); }
inline u32 SamplerStageCount(u32, bool = false) { return CTexture::mtMaxCombinedShaderTextures; }
inline void DeleteTextures(GLsizei count, const GLuint* textures)
{
    CHK_GL(glDeleteTextures(count, textures));
    for (GLsizei index = 0; index < count; ++index)
    {
        if (textures[index] != 0)
        {
            GLHardwareState::InvalidateAttachments();
            break;
        }
    }
}
inline void DeleteSamplers(GLsizei count, const GLuint* samplers) { CHK_GL(glDeleteSamplers(count, samplers)); }
#endif
} // namespace xray::render::RENDER_NAMESPACE::GLTextureBindings
