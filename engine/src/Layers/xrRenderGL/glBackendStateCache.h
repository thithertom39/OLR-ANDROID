#pragma once

#include <array>

namespace xray::render::RENDER_NAMESPACE::GLHardwareState
{
// Command lists keep their logical state, but all of them submit to one GL
// context. Only this cache may decide whether a hardware change is redundant.
// Zero is a valid binding; validity must be represented separately.
template <typename T>
struct CachedValue
{
    T value{};
    bool valid = false;

    bool update(const T& desired, bool force = false)
    {
        if (!force && valid && value == desired)
            return false;
        value = desired;
        valid = true;
        return true;
    }

    void invalidate() { valid = false; }
};

struct State
{
    CachedValue<GLuint> framebuffer;
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    struct DrawBuffers
    {
        std::array<GLenum, 4> values{};
        GLsizei count = 0;
        bool valid = false;
    } draw_buffers;
#endif
    std::array<CachedValue<GLuint>, 4> color_attachments;
    CachedValue<GLuint> depth_attachment;
    CachedValue<GLuint> program;
    CachedValue<GLuint> vertex_array;
    CachedValue<GLuint> index_buffer;
    GLuint vertex_buffer = 0;
    u32 vertex_stride = 0;
    size_t vertex_offset = 0;
    bool vertex_valid = false;

    CachedValue<u32> cull_mode;
    CachedValue<u32> depth_enable;
    CachedValue<u32> depth_func;
    CachedValue<u32> color_mask;
    CachedValue<u32> fill_mode;
    CachedValue<std::array<u32, 8>> stencil;
    CachedValue<bool> depth_write;
    CachedValue<bool> blend_enable;
    CachedValue<std::array<GLenum, 4>> blend_args;
    CachedValue<std::array<GLenum, 2>> blend_equations;
};

inline State& Get()
{
    // This renderer has one context per rendering thread. CPU command-list
    // resets do not touch this state and cannot invalidate another thread.
    static thread_local State state;
    return state;
}

// Draw selection is FBO state, independent of the attached texture names.
// Every physical DRAW/FRAMEBUFFER bind or direct selection mutation must
// invalidate it; logical command-list changes alone cannot establish it.
inline void InvalidateDrawBuffers()
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    Get().draw_buffers.valid = false;
#endif
}

inline void SetDrawBuffers(GLsizei count, const GLenum* buffers, bool force = false)
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    auto& state = Get();
    // GLES 3 guarantees four draw buffers. Cache only unquestionably valid
    // selections for the known nondefault FBO; other calls retain GL validation
    // and errors. The physical FBO uses the existing valid-bind contract.
    bool cacheable = state.framebuffer.valid && state.framebuffer.value != 0 &&
        count >= 0 && count <= 4 && (count == 0 || buffers != nullptr);
    if (cacheable)
        for (GLsizei i = 0; i < count; ++i)
            if (buffers[i] != GL_NONE && buffers[i] != GL_COLOR_ATTACHMENT0 + GLenum(i))
                cacheable = false;
    if (!cacheable)
    {
        InvalidateDrawBuffers();
        CHK_GL(glDrawBuffers(count, buffers));
        return;
    }

    auto& selection = state.draw_buffers;
    bool same = selection.valid && selection.count == count;
    for (GLsizei i = 0; same && i < count; ++i)
        same = selection.values[i] == buffers[i];
    if (!force && same)
        return;

    CHK_GL(glDrawBuffers(count, buffers));
    for (GLsizei i = 0; i < count; ++i)
        selection.values[i] = buffers[i];
    selection.count = count;
    selection.valid = true;
#else
    (void)force;
    CHK_GL(glDrawBuffers(count, buffers));
#endif
}

inline void InvalidateAttachments()
{
    auto& state = Get();
    for (auto& attachment : state.color_attachments)
        attachment.invalidate();
    state.depth_attachment.invalidate();
}

inline void InvalidateGeometry()
{
    auto& state = Get();
    state.vertex_array.invalidate();
    state.index_buffer.invalidate();
    state.vertex_valid = false;
}

inline void Invalidate() { Get() = State{}; }

inline void SetDepthMask(bool enabled, bool force = false)
{
    if (Get().depth_write.update(enabled, force))
        CHK_GL(glDepthMask(enabled ? GL_TRUE : GL_FALSE));
}
} // namespace xray::render::RENDER_NAMESPACE::GLHardwareState
