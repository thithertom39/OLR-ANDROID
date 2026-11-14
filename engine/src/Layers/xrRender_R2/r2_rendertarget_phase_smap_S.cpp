#include "stdafx.h"

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
#include <cstring>
#include "Layers/xrRenderGL/glSpotShadowDump.h"
#endif

namespace xray::render::RENDER_NAMESPACE
{
// See r3_rendertarget_phase_smap_D.cpp: on GL the shadow maps are depth-only, so
// the 2048x2048 RGBA8 rt_smap_surf is not attached. A tsh fill writes into that
// color target, and tsh is a DX-era path that is off unless -tsh is passed.
#if defined(USE_OGL)
#   define OXR_SMAP_COLOR_ATTACHMENT nullptr
#else
#   define OXR_SMAP_COLOR_ATTACHMENT rt_smap_surf
#endif

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
// Shared with the geometry and accumulation audit in the two spot-light files.
// These hooks run on the render thread. The optional depth dump restores and
// verifies temporary GL bindings/state before returning; the other queries do
// not change them.
u32 gles_spot_shadow_audit_ticket(const light& source)
{
    static const bool enabled = []
    {
        if (!Core.Params)
            return false;
        constexpr char flag[] = "-gles_spot_shadow_audit";
        constexpr size_t length = sizeof(flag) - 1;
        for (pcstr arg = Core.Params; (arg = std::strstr(arg, flag)) != nullptr; ++arg)
        {
            if ((arg == Core.Params || std::strchr(" \t\r\n", arg[-1])) &&
                (arg[length] == '\0' || std::strchr(" \t\r\n", arg[length])))
                return true;
        }
        return false;
    }();
    if (!enabled || source.flags.type != IRender_Light::SPOT || !source.flags.bShadow)
        return 0;

    // Keep a later-enabled player torch observable even if another spot light
    // rendered first. At most eight light pointers and two frames each are logged.
    struct Entry
    {
        const light* source{};
        u32 frame{};
        u32 frames_logged{};
        u32 ticket{};
    };
    static Entry entries[8]{};
    static u32 next_ticket = 0;
    for (auto& entry : entries)
    {
        if (!entry.source)
        {
            entry = { &source, Device.dwFrame, 1, ++next_ticket };
            return entry.ticket;
        }
        if (entry.source != &source)
            continue;
        if (entry.frame != Device.dwFrame)
        {
            if (entry.frames_logged >= 2)
                return 0;
            entry.frame = Device.dwFrame;
            ++entry.frames_logged;
            entry.ticket = ++next_ticket;
        }
        return entry.ticket;
    }
    return 0;
}

void gles_spot_shadow_audit_framebuffer(const CBackend& cmd_list, const light& source,
    const CRT& desired_depth, u32 ticket, pcstr stage)
{
    static u32 snapshots_logged = 0;
    if (!ticket || snapshots_logged >= 32)
        return;
    ++snapshots_logged;
    const GLenum attachments[] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
        GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT
    };
    GLint types[5]{};
    GLint names[5]{};
    for (u32 i = 0; i < 5; ++i)
    {
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachments[i],
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &types[i]);
        if (types[i] != GL_NONE)
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachments[i],
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &names[i]);
    }
    GLint framebuffer = 0, viewport[4]{}, depth_func = 0, scissor_box[4]{};
    GLboolean depth_write = GL_FALSE;
    GLfloat depth_range[2]{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glGetFloatv(GL_DEPTH_RANGE, depth_range);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    const bool colors_detached = types[0] == GL_NONE && types[1] == GL_NONE && types[2] == GL_NONE;
    const bool depth_matches = types[3] == GL_TEXTURE && GLuint(names[3]) == desired_depth.pZRT;
    const u32 smapsize = RImplementation.o.smapsize;
    const bool atlas_fits = source.X.S.posX <= smapsize && source.X.S.posY <= smapsize &&
        source.X.S.size <= smapsize - source.X.S.posX && source.X.S.size <= smapsize - source.X.S.posY;
    Msg("* [gles_spot_shadow_audit] ticket=%u stage=%s frame=%u light=%p type=%u static=%u context=%u "
        "atlas=%u,%u,%u smapsize=%u atlas_fits=%u fbo=%d status=0x%04x viewport=%d,%d,%d,%d "
        "desired_depth=%u colors_detached=%u depth_matches=%u depth_test=%u depth_write=%u "
        "depth_func=0x%04x depth_range=%g,%g scissor=%u scissor_box=%d,%d,%d,%d",
        ticket, stage, Device.dwFrame, static_cast<const void*>(&source), u32(source.flags.type),
        u32(source.flags.bStatic), cmd_list.context_id, source.X.S.posX, source.X.S.posY, source.X.S.size,
        smapsize, u32(atlas_fits), framebuffer,
        u32(status), viewport[0], viewport[1], viewport[2], viewport[3], desired_depth.pZRT,
        u32(colors_detached), u32(depth_matches), u32(glIsEnabled(GL_DEPTH_TEST)), u32(depth_write),
        u32(depth_func), depth_range[0], depth_range[1], u32(glIsEnabled(GL_SCISSOR_TEST)),
        scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
    Msg("* [gles_spot_shadow_audit] ticket=%u stage=%s color0=0x%04x:%d color1=0x%04x:%d "
        "color2=0x%04x:%d depth=0x%04x:%d stencil=0x%04x:%d position=%g,%g,%g range=%g near=%g cone=%g",
        ticket, stage, u32(types[0]), names[0], u32(types[1]), names[1], u32(types[2]), names[2],
        u32(types[3]), names[3], u32(types[4]), names[4], source.position.x, source.position.y,
        source.position.z, source.range, source.virtual_size, source.cone);
    if (!source.flags.bStatic && source.flags.type == IRender_Light::SPOT && source.flags.bShadow &&
        std::strcmp(stage, "after_opaque") == 0 && status == GL_FRAMEBUFFER_COMPLETE &&
        depth_matches && atlas_fits)
    {
        GLSpotShadowDump::Capture(ticket, Device.dwFrame, cmd_list.context_id, GLuint(names[3]),
            source.X.S.posX, source.X.S.posY, source.X.S.size, smapsize);
    }
}
#endif

void CRenderTarget::phase_smap_spot_clear(CBackend& cmd_list)
{
    rt_smap_depth->set_slice_write(cmd_list.context_id, 0);
    cmd_list.set_pass_targets(
        OXR_SMAP_COLOR_ATTACHMENT,
        nullptr,
        nullptr,
        rt_smap_depth
    );
    cmd_list.ClearZB(rt_smap_depth, 1.0f);

#if defined(USE_DX11)
    HW.get_context(CHW::IMM_CTX_ID)->ClearState();
#endif
}

void CRenderTarget::phase_smap_spot(CBackend& cmd_list, light* L)
{
    const D3D_VIEWPORT viewport = { L->X.S.posX, L->X.S.posY, L->X.S.size, L->X.S.size, 0.f, 1.f };
    rt_smap_depth->set_slice_write(cmd_list.context_id, 0); // TODO: it is possible to increase lights batch size
                                                            // by rendering into different smap array slices in parallel
    cmd_list.set_pass_targets(
        OXR_SMAP_COLOR_ATTACHMENT,
        nullptr,
        nullptr,
        rt_smap_depth
#if defined(USE_OGL)
        , &viewport
#endif
    );
#if !defined(USE_OGL)
    cmd_list.SetViewport(viewport);
#endif

    // Misc		- draw only front-faces //back-faces
    cmd_list.set_CullMode(CULL_CCW);
    cmd_list.set_Stencil(FALSE);
    // no transparency
#pragma todo("can optimize for multi-lights covering more than say 50%...")
    if (RImplementation.o.HW_smap)
        cmd_list.set_ColorWriteEnable(FALSE);
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
    gles_spot_shadow_audit_framebuffer(cmd_list, *L, *rt_smap_depth,
        gles_spot_shadow_audit_ticket(*L), "setup");
#endif
}

void CRenderTarget::phase_smap_spot_tsh(CBackend& cmd_list, light* L)
{
    VERIFY(!"Implement clear of the buffer for tsh!");
    VERIFY(RImplementation.o.Tshadows);
    cmd_list.set_ColorWriteEnable();
#if defined(USE_OGL)
    // Depth-only shadow FBO on GL: there is no color target for the tsh fill to
    // rasterize into. tsh is unimplemented on this backend anyway (see the
    // VERIFY above) and stays off unless -tsh is passed.
#else
    if (IRender_Light::OMNIPART == L->flags.type)
    {
        // omni-part
        cmd_list.ClearRT(cmd_list.get_RT(), { 1.0f, 1.0f, 1.0f, 1.0f });
    }
    else
    {
        // real-spot
        // Select color-mask
        ref_shader shader = L->s_spot;
        if (!shader)
            shader = s_accum_spot;
        cmd_list.set_Element(shader->E[SE_L_FILL]);

        // Fill vertex buffer
        Fvector2 p0, p1;
        u32 Offset;
        u32 C = color_rgba(255, 255, 255, 255);
        float _w = float(L->X.S.size);
        float _h = float(L->X.S.size);
        float d_Z = EPS_S;
        float d_W = 1.f;
        p0.set(.5f / _w, .5f / _h);
        p1.set((_w + .5f) / _w, (_h + .5f) / _h);

        FVF::TL* pv = (FVF::TL*)RImplementation.Vertex.Lock(4, g_combine->vb_stride, Offset);
#if defined(USE_DX11)
        pv->set(EPS, float(_h + EPS), d_Z, d_W, C, p0.x, p1.y);
        pv++;
        pv->set(EPS, EPS, d_Z, d_W, C, p0.x, p0.y);
        pv++;
        pv->set(float(_w + EPS), float(_h + EPS), d_Z, d_W, C, p1.x, p1.y);
        pv++;
        pv->set(float(_w + EPS), EPS, d_Z, d_W, C, p1.x, p0.y);
        pv++;
#elif defined(USE_OGL)
        pv->set(EPS, EPS, d_Z, d_W, C, p0.x, p0.y);
        pv++;
        pv->set(EPS, float(_h + EPS), d_Z, d_W, C, p0.x, p1.y);
        pv++;
        pv->set(float(_w + EPS), EPS, d_Z, d_W, C, p1.x, p0.y);
        pv++;
        pv->set(float(_w + EPS), float(_h + EPS), d_Z, d_W, C, p1.x, p1.y);
        pv++;
#else
#   error No graphics API selected or enabled!
#endif
        RImplementation.Vertex.Unlock(4, g_combine->vb_stride);
        cmd_list.set_Geometry(g_combine);

        // draw
        cmd_list.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
    }
#endif // USE_OGL
}
} // namespace xray::render::RENDER_NAMESPACE
#undef OXR_SMAP_COLOR_ATTACHMENT
