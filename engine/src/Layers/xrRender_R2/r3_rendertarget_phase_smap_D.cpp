#include "stdafx.h"

namespace xray::render::RENDER_NAMESPACE
{
// The GL backend renders the shadow maps depth-only. rt_smap_surf exists purely
// to give the FBO a color attachment, and at 2048x2048 with a D3DFMT_R5G6B5
// format that GL expands to RGBA8 it is a 16 MB texture that the sun pass then
// loads and stores for nothing, once per cascade and once per spot light. A
// depth-only FBO is legal in GLES 3.x, so skip the attachment entirely.
#if defined(USE_OGL)
#   define OXR_SMAP_COLOR_ATTACHMENT nullptr
#else
#   define OXR_SMAP_COLOR_ATTACHMENT rt_smap_surf
#endif

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
namespace
{
void detach_sun_shadow_colors(CBackend& cmd_list)
{
    static const bool enabled = strstr(Core.Params, "-gles_shadow_detach") != nullptr;
    if (!enabled)
        return;

    // Keep the diagnostic unconditional detach available for comparison.
    // The normal path now tracks these attachments across command lists.
    cmd_list.set_FB(HW.pFB);
    for (u32 i = 0; i < 3; ++i)
        CHK_GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0));
    GLHardwareState::InvalidateAttachments();
}

void audit_sun_shadow_framebuffer(const CBackend& cmd_list, u32 cascade, const CRT& desired_depth)
{
    static const bool enabled = strstr(Core.Params, "-gles_shadow_audit") != nullptr;
    static u32 passes_logged = 0;
    if (!enabled || passes_logged >= 6)
        return;
    ++passes_logged;

    const GLenum attachments[] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
        GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT
    };
    GLint types[5] = {};
    GLint names[5] = {};
    for (u32 i = 0; i < 5; ++i)
    {
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachments[i],
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &types[i]);
        if (types[i] != GL_NONE)
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachments[i],
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &names[i]);
    }

    GLint framebuffer = 0;
    GLint viewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    const bool colors_detached = types[0] == GL_NONE && types[1] == GL_NONE && types[2] == GL_NONE;
    const bool depth_matches = types[3] == GL_TEXTURE && GLuint(names[3]) == desired_depth.pZRT;

    Msg("* [gles_shadow_audit] pass=%u cascade=%u context=%u fbo=%d status=0x%04x "
        "viewport=%d,%d,%d,%d desired_depth=%u desired_size=%ux%u colors_detached=%u depth_matches=%u",
        passes_logged, cascade, cmd_list.context_id, framebuffer, u32(status),
        viewport[0], viewport[1], viewport[2], viewport[3], desired_depth.pZRT,
        desired_depth.dwWidth, desired_depth.dwHeight, u32(colors_detached), u32(depth_matches));
    Msg("* [gles_shadow_audit] color0(type=0x%04x,name=%d) color1(type=0x%04x,name=%d) "
        "color2(type=0x%04x,name=%d) depth(type=0x%04x,name=%d) stencil(type=0x%04x,name=%d)",
        u32(types[0]), names[0], u32(types[1]), names[1], u32(types[2]), names[2],
        u32(types[3]), names[3], u32(types[4]), names[4]);
}
} // namespace
#endif

void CRenderTarget::phase_smap_direct(CBackend& cmd_list, light *L, u32 sub_phase)
{
    if (sub_phase == SE_SUN_RAIN_SMAP)
    {
        u_setrt(cmd_list, nullptr, nullptr, nullptr, rt_smap_rain);
        cmd_list.ClearZB(rt_smap_rain, 1.0f);
        cmd_list.SetViewport({0, 0, rt_smap_rain->dwWidth, rt_smap_rain->dwHeight, 0.0, 1.0});
    }
    else
    {
        rt_smap_depth->set_slice_write(cmd_list.context_id, sub_phase);
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
        detach_sun_shadow_colors(cmd_list);
#endif
        cmd_list.set_pass_targets(
            OXR_SMAP_COLOR_ATTACHMENT,
            nullptr,
            nullptr,
            rt_smap_depth
        );
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
        audit_sun_shadow_framebuffer(cmd_list, sub_phase, *rt_smap_depth);
#endif
        cmd_list.ClearZB(rt_smap_depth, 1.0f);
    }

    // Stencil	- disable
    cmd_list.set_Stencil(FALSE);
}

void CRenderTarget::phase_smap_direct_tsh(CBackend& cmd_list, light *L, u32 sub_phase)
{
    VERIFY(RImplementation.o.Tshadows);
    cmd_list.set_ColorWriteEnable();
    //	Prepare viewport for shadow map rendering
    RImplementation.rmNormal(cmd_list);
#if !defined(USE_OGL)
    cmd_list.ClearRT(cmd_list.get_RT(), { 1.0f, 1.0f, 1.0f, 1.0f }); // color_rgba(127, 127, 12, 12);
#endif
}
} // namespace xray::render::RENDER_NAMESPACE
#undef OXR_SMAP_COLOR_ATTACHMENT
