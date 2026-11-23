#pragma once

namespace xray::render::RENDER_NAMESPACE
{
struct VertexBindingOptions
{
    bool separated = false;
    bool eager = true;
};
inline VertexBindingOptions& VertexBindingState()
{
    static VertexBindingOptions options;
    return options;
}
inline void InitializeVertexBinding()
{
    auto& options = VertexBindingState();
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    const bool legacy = Core.Params && strstr(Core.Params, "-gles_legacy_vertex_binding") != nullptr;
    options.separated = !legacy && (GLAD_GL_ES_VERSION_3_1 || GLAD_GL_ES_VERSION_3_2) &&
        glBindVertexBuffer && glVertexAttribFormat && glVertexAttribBinding;
    options.eager = Core.Params && strstr(Core.Params, "-gles_eager_vertex_binding") != nullptr;
#else
    options.separated = GLAD_GL_ARB_vertex_attrib_binding;
    options.eager = true;
#endif
}
// ES 3.1 has these functions in core; it does not advertise the desktop ARB
// extension. Declaration setup and buffer rebinding must select the same API.
inline bool UseSeparatedVertexBinding()
{
    return VertexBindingState().separated;
}
}
