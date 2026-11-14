#pragma once

#include <cstring>
#include <cstddef>

namespace xray::render::RENDER_NAMESPACE::GLStreamSync
{
// Opt-in comparison with driver-managed mapping synchronization. The former
// -gles_stream_orphan experiment is intentionally no longer recognized: its
// unsynchronized writes produced corrupted HUD geometry on Adreno.
inline bool HasFlag(pcstr flag)
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    if (!Core.Params)
        return false;
    const auto length = std::strlen(flag);
    for (pcstr arg = Core.Params; (arg = std::strstr(arg, flag)) != nullptr; ++arg)
    {
        if ((arg == Core.Params || std::strchr(" \t\r\n", arg[-1]) != nullptr) &&
            std::strchr(" \t\r\n", arg[length]) != nullptr)
            return true;
    }
    return false;
#else
    return false;
#endif
}

inline bool OrphaningEnabled()
{
    static const bool enabled = HasFlag("-gles_stream_sync");
    return enabled;
}

inline bool UploadEnabled()
{
    static const bool enabled = HasFlag("-gles_stream_upload");
    return enabled;
}

inline bool BypassFrameFence()
{
    return UploadEnabled() || OrphaningEnabled();
}

inline void ReplaceStorage(GLenum target, std::size_t capacity, std::size_t offset, std::size_t size)
{
    // Keep the object name referenced by the VAO/geometry. Earlier draw calls
    // retain their data store; all following writes append to the replacement.
    CHECK_OR_EXIT(capacity > 0 && size > 0 && offset <= capacity && size <= capacity - offset,
        "GPU stream replacement range is invalid");
    // Report pre-existing errors separately, so an old error is not attributed
    // to storage replacement. Check this infrequent allocation even in Release.
    GLenum priorError = glGetError();
    for (unsigned i = 0; priorError != GL_NO_ERROR && i < 8; ++i)
    {
        Msg("! [GLES stream] preceding GL error=0x%x target=0x%x", priorError, target);
        priorError = glGetError();
    }
    CHECK_OR_EXIT(priorError == GL_NO_ERROR, "GPU stream: could not clear preceding GL errors");
    glBufferData(target, static_cast<GLsizeiptr>(capacity), nullptr, GL_DYNAMIC_DRAW);
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        Msg("! [GLES stream] storage failed: target=0x%x capacity=%zu offset=%zu size=%zu error=0x%x",
            target, capacity, offset, size, error);
    CHECK_OR_EXIT(error == GL_NO_ERROR, "GPU stream storage allocation failed (see log)");
}

inline bool OrphanOnFlush(GLenum target, bool flush, std::size_t offset, std::size_t size)
{
    if (!flush || !OrphaningEnabled())
        return false;
    GLint capacity = 0;
    CHK_GL(glGetBufferParameteriv(target, GL_BUFFER_SIZE, &capacity));
    CHECK_OR_EXIT(capacity > 0, "GPU stream capacity is invalid");
    ReplaceStorage(target, static_cast<std::size_t>(capacity), offset, size);
    return true;
}

inline void CheckUploadCreation(GLuint buffer, std::size_t capacity)
{
    const GLenum error = glGetError();
    if (!buffer || error != GL_NO_ERROR)
        Msg("! [GLES stream] create check failed: buffer=%u capacity=%zu error=0x%x", buffer, capacity, error);
    CHECK_OR_EXIT(buffer != 0 && error == GL_NO_ERROR, "GPU upload stream creation failed (see log)");
}

inline void UploadRange(GLenum target, GLuint buffer, std::size_t offset, std::size_t size, const void* data)
{
    // BufferSubData consumes client memory before returning. Later CPU writes
    // cannot modify the bytes already queued for an earlier draw.
    glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    // Audit startup without adding a driver query to every object in every
    // frame. Capacity/lifetime checks remain enabled for every CPU mapping.
    static unsigned checkedUploads = 0;
    if (checkedUploads < 8)
    {
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR)
            Msg("! [GLES stream] upload check failed: target=0x%x buffer=%u offset=%zu size=%zu error=0x%x",
                target, buffer, offset, size, error);
        CHECK_OR_EXIT(error == GL_NO_ERROR, "GPU stream upload check failed (see log)");
        if (++checkedUploads == 8)
            Msg("* [gles_upload_audit] First 8 BufferSubData uploads passed GL error checks");
    }
}

inline void CheckMap(void* pointer, GLenum target, GLuint buffer, std::size_t offset, std::size_t size)
{
    if (!OrphaningEnabled() || pointer)
        return;
    const GLenum error = glGetError();
    Msg("! [GLES stream] map failed: target=0x%x buffer=%u offset=%zu size=%zu error=0x%x",
        target, buffer, offset, size, error);
    CHECK_OR_EXIT(false, "GPU stream mapping failed (see log)");
}

inline void CheckUnmap(GLboolean result, GLenum target, GLuint buffer)
{
    if (!OrphaningEnabled() || result == GL_TRUE)
        return;
    const GLenum error = glGetError();
    Msg("! [GLES stream] unmap failed: target=0x%x buffer=%u result=%u error=0x%x",
        target, buffer, static_cast<unsigned>(result), error);
    CHECK_OR_EXIT(false, "GPU stream contents were lost during unmap (see log)");
}
} // namespace xray::render::RENDER_NAMESPACE::GLStreamSync
