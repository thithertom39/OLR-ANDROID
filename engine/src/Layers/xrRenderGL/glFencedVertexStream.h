#pragma once

#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
#include <SDL_video.h>
#include <array>
#include <atomic>
#include <cstring>

namespace xray::render::RENDER_NAMESPACE::GLFencedVertexStream
{
struct Binding
{
    GLuint buffer = 0;
    std::size_t base = 0;
};
// The dynamic vertex stream and its GL geometry submissions run on the same
// context thread. The logical vertex offsets visible to draw callers stay unchanged.
inline thread_local Binding binding;
inline std::atomic<GLuint> streamBuffer{0};

inline std::size_t Base(GLuint buffer)
{
    // Indexed draws for immutable geometry never need Android's emulated TLS.
    if (!buffer || buffer != streamBuffer.load(std::memory_order_relaxed))
        return 0;
    return binding.buffer == buffer ? binding.base : 0;
}

class Arena
{
    // Slot zero is reserved for synchronized uploads through BufferSubData.
    static constexpr unsigned MappedSlots = 3;
    static constexpr GLbitfield Persistent = 0x0040;
    static constexpr GLbitfield Coherent = 0x0080;
    static constexpr GLbitfield Dynamic = 0x0100;
    std::array<GLsync, MappedSlots> fences{};
    SDL_GLContext context = nullptr;
    u8* data = nullptr;
    std::size_t stride = 0;
    unsigned active = 0;
    unsigned next = 0;
    bool fenceFailed = false;

    static bool Supported()
    {
        if (!glGetStringi || !glMapBufferRange || !glFenceSync || !glClientWaitSync || !glDeleteSync)
            return false;
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; ++i)
        {
            const auto* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, GLuint(i)));
            if (name && std::strcmp(name, "GL_EXT_buffer_storage") == 0)
                return true;
        }
        return false;
    }

public:
    unsigned mappedDiscards = 0, fallbackDiscards = 0;

    bool Create(GLuint& buffer, std::size_t capacity)
    {
        const auto current = SDL_GL_GetCurrentContext();
        if (!current || binding.buffer || !Supported())
            return false;
        using Storage = void (*)(GLenum, GLsizeiptr, const void*, GLbitfield);
        const auto storage = reinterpret_cast<Storage>(SDL_GL_GetProcAddress("glBufferStorageEXT"));
        if (!storage)
            return false;

        const auto priorError = glGetError();
        if (priorError != GL_NO_ERROR)
        {
            Msg("! [gles_fenced_vertex_stream] preceding GL error=0x%x", priorError);
            return false;
        }
        GLuint candidate = 0;
        glGenBuffers(1, &candidate);
        GLenum error = glGetError();
        if (candidate && error == GL_NO_ERROR)
        {
            glBindBuffer(GL_ARRAY_BUFFER, candidate);
            constexpr auto access = GL_MAP_WRITE_BIT | Persistent | Coherent;
            storage(GL_ARRAY_BUFFER, GLsizeiptr(capacity * (MappedSlots + 1)), nullptr, access | Dynamic);
            error = glGetError();
            if (error == GL_NO_ERROR)
            {
                data = static_cast<u8*>(glMapBufferRange(GL_ARRAY_BUFFER, GLintptr(capacity),
                    GLsizeiptr(capacity * MappedSlots), access));
                error = glGetError();
            }
        }
        if (!data || error != GL_NO_ERROR)
        {
            if (candidate)
                glDeleteBuffers(1, &candidate);
            data = nullptr;
            Msg("! [gles_fenced_vertex_stream] unavailable, using ordered uploads (error=0x%x)", error);
            return false;
        }
        context = current;
        stride = capacity;
        buffer = candidate;
        binding = {buffer, 0};
        streamBuffer.store(buffer, std::memory_order_relaxed);
        Msg("* [gles_fenced_vertex_stream] mapped_slots=%u slot_bytes=%zu", MappedSlots, capacity);
        return true;
    }

    bool Enabled() const { return data != nullptr; }
    bool MappedSlot() const { return active != 0; }
    u8* Address(std::size_t offset) { return data + std::size_t(active - 1) * stride + offset; }

    void Discard(GLuint buffer)
    {
        // Discard is called at the Lock which replaces the old logical store.
        // All draws sourcing it have already been queued, including those
        // which reused cached offsets after a frame's Flush marker.
        if (active)
        {
            auto& fence = fences[active - 1];
            fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            fenceFailed |= fence == nullptr;
        }
        active = 0;
        if (!fenceFailed)
        {
            for (unsigned i = 0; i < MappedSlots; ++i)
            {
                const auto slot = (next + i) % MappedSlots;
                auto& fence = fences[slot];
                if (fence)
                {
                    const auto status = glClientWaitSync(fence, 0, 0);
                    if (status == GL_WAIT_FAILED)
                    {
                        fenceFailed = true;
                        break;
                    }
                    if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED)
                        continue;
                    glDeleteSync(fence);
                    fence = nullptr;
                }
                active = slot + 1;
                next = (slot + 1) % MappedSlots;
                break;
            }
        }
        binding = {buffer, std::size_t(active) * stride};
        if (active)
            ++mappedDiscards;
        else
            ++fallbackDiscards;
    }

    bool Release(GLuint buffer)
    {
        const bool current = !context || context == SDL_GL_GetCurrentContext();
        if (current)
            for (const auto fence : fences)
                if (fence)
                    glDeleteSync(fence);
        if (binding.buffer == buffer)
        {
            streamBuffer.store(0, std::memory_order_relaxed);
            binding = {};
        }
        *this = {};
        return current;
    }
};
} // namespace xray::render::RENDER_NAMESPACE::GLFencedVertexStream
#endif
