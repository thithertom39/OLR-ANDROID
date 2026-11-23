#pragma once

namespace xray::render::RENDER_NAMESPACE
{
// ES 3.1 has these functions in core; it does not advertise the desktop ARB
// extension. Declaration setup and buffer rebinding must select the same API.
inline bool UseSeparatedVertexBinding()
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    const bool legacy = Core.Params && strstr(Core.Params, "-gles_legacy_vertex_binding") != nullptr;
    return !legacy && (GLAD_GL_ES_VERSION_3_1 || GLAD_GL_ES_VERSION_3_2) &&
        glBindVertexBuffer && glVertexAttribFormat && glVertexAttribBinding;
#else
    return GLAD_GL_ARB_vertex_attrib_binding;
#endif
}
}
