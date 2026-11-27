#pragma once

#include "Layers/xrRender/FLOD.h"
#include "glBackendStateCache.h"
#include <SDL_video.h>

namespace xray::render::RENDER_NAMESPACE::GLLodStream
{
struct State
{
    SDL_GLContext context = nullptr;
    GLuint buffer = 0;
    bool failed = false;
    xr_vector<FLOD::_hw> vertices;
};

inline State& Get()
{
    static State state;
    return state;
}

inline void Shutdown()
{
    auto& state = Get();
    if (state.buffer && state.context == SDL_GL_GetCurrentContext())
    {
        CHK_GL(glDeleteBuffers(1, &state.buffer));
        GLHardwareState::InvalidateGeometry();
    }
    state = {};
}

inline FLOD::_hw* Reserve(size_t vertices)
{
    if (const auto current = SDL_GL_GetCurrentContext())
    {
        auto& state = Get();
        if (state.context != current)
            Shutdown();
        state.context = current;
        if (!state.failed)
        {
            state.vertices.resize(vertices);
            return state.vertices.data();
        }
    }
    return nullptr;
}

inline GLuint Upload()
{
    auto& state = Get();
    if (!state.buffer)
        glGenBuffers(1, &state.buffer);
    if (state.buffer)
    {
        glBindBuffer(GL_ARRAY_BUFFER, state.buffer);
        // This stream is used only by LOD geometry. A new data store allows
        // already queued draws to retain their original vertex data.
        const auto bytes = static_cast<GLsizeiptr>(state.vertices.size() * sizeof(FLOD::_hw));
        glBufferData(GL_ARRAY_BUFFER, bytes, state.vertices.data(), GL_STREAM_DRAW);
    }
    const GLenum error = glGetError();
    if (state.buffer && error == GL_NO_ERROR)
        return state.buffer;
    Msg("! [gles_lod_stream] dedicated upload unavailable, buffer=%u error=0x%x", state.buffer, error);
    state.failed = true;
    return 0;
}
} // namespace xray::render::RENDER_NAMESPACE::GLLodStream
