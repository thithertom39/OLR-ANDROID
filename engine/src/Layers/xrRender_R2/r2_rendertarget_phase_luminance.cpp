#include "stdafx.h"
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "Layers/xrRenderGL/glGpuProfiler.h"
#endif

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
#include <cmath>
#include <cstring>
#endif

namespace xray::render::RENDER_NAMESPACE
{
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
bool gles_lighting_audit_current_frame();
#endif

namespace phase_luminance
{
#pragma pack(push, 4)
struct v_build
{
    Fvector4 p;
    Fvector2 uv0;
    Fvector2 uv1;
    Fvector2 uv2;
    Fvector2 uv3;
};

struct v_filter
{
    Fvector4 p;
    Fvector4 uv[8];
};
#pragma pack(pop)

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
namespace angle_audit
{
bool Enabled()
{
    static const bool enabled = []
    {
        if (!Core.Params)
            return false;
        constexpr char flag[] = "-gles_angle_audit";
        constexpr size_t length = sizeof(flag) - 1;
        for (pcstr arg = Core.Params; (arg = std::strstr(arg, flag)) != nullptr; ++arg)
        {
            if ((arg == Core.Params || std::strchr(" \t\r\n", arg[-1])) &&
                (arg[length] == '\0' || std::strchr(" \t\r\n", arg[length])))
                return true;
        }
        return false;
    }();
    return enabled;
}

u32 NextSample(u32 gpu)
{
    if (!Enabled())
        return 0;
    static u32 visits[CHWCaps::MAX_GPUS]{};
    static u32 samples = 0;
    if (samples >= 8 || gpu >= CHWCaps::MAX_GPUS)
        return 0;
    const u32 visit = ++visits[gpu];
    // Two initial samples of each alternating exposure history, then two
    // later samples. The process-wide budget also bounds unusual GPU counts.
    if (visit != 1 && visit != 2 && visit != 16 && visit != 64)
        return 0;
    return ++samples;
}

bool CheckErrors(u32 ticket, pcstr phase, pcstr stage)
{
    bool clean = true;
    for (u32 i = 0; i < 8; ++i)
    {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            return clean;
        clean = false;
        Msg("! [gles_angle_audit] luminance ticket=%u phase=%s stage=%s error=0x%x",
            ticket, phase, stage, u32(error));
    }
    Msg("! [gles_angle_audit] luminance ticket=%u phase=%s stage=%s error_drain_limit=8",
        ticket, phase, stage);
    return false;
}

void Probe(u32 ticket, u32 gpu, pcstr phase, const CRT& target)
{
    // Opt-in synchronous readback is diagnostic only, never a performance
    // measurement. Incoming GL errors are consumed and labelled separately.
    CheckErrors(ticket, phase, "incoming");
    constexpr GLenum packNames[] = {
        GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS
    };
    GLint drawFbo = 0, readFbo = 0, oldReadBuffer = 0, packBuffer = 0, samples = 0;
    GLint pack[4]{};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glGetIntegerv(GL_READ_BUFFER, &oldReadBuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
    glGetIntegerv(GL_SAMPLES, &samples);
    for (u32 i = 0; i < 4; ++i)
        glGetIntegerv(packNames[i], &pack[i]);
    const GLenum drawStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    GLint type = GL_NONE, name = 0, level = -1, component = GL_NONE;
    if (drawFbo && drawStatus == GL_FRAMEBUFFER_COMPLETE)
    {
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        if (type == GL_TEXTURE)
        {
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component);
        }
    }
    const bool queryClean = CheckErrors(ticket, phase, "target_queries");
    Msg("* [gles_angle_audit] luminance ticket=%u frame=%u gpu=%u phase=%s draw_fbo=%d read_fbo=%d "
        "status=0x%x texture=%d expected=%u level=%d component=0x%x size=%ux%u samples=%d",
        ticket, Device.dwFrame, gpu, phase, drawFbo, readFbo, u32(drawStatus), name, target.pRT,
        level, u32(component), target.dwWidth, target.dwHeight, samples);
    if (!queryClean || !drawFbo || drawStatus != GL_FRAMEBUFFER_COMPLETE || type != GL_TEXTURE ||
        GLuint(name) != target.pRT || !target.pRT || level != 0 || component != GL_FLOAT ||
        target.dwWidth != 1 || target.dwHeight != 1 || target.fmt != D3DFMT_R32F || samples != 0)
    {
        Msg("! [gles_angle_audit] luminance ticket=%u phase=%s readback=unsupported reason=target_validation",
            ticket, phase);
        return;
    }

    // Bind only READ; the draw binding, attachments and RCache remain intact.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(drawFbo));
    GLint targetReadBuffer = 0;
    glGetIntegerv(GL_READ_BUFFER, &targetReadBuffer);
    const bool targetReadSaved = CheckErrors(ticket, phase, "save_target_read_buffer");
    bool readBufferChanged = false;
    bool packChanged = false;
    if (targetReadSaved)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        readBufferChanged = true;
        const GLenum readStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        GLint format = GL_NONE, pixelType = GL_NONE;
        if (readStatus == GL_FRAMEBUFFER_COMPLETE)
        {
            glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
            glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &pixelType);
        }
        const bool readQueryClean = CheckErrors(ticket, phase, "read_format_queries");
        Msg("* [gles_angle_audit] luminance ticket=%u phase=%s read_status=0x%x implementation_format=0x%x "
            "implementation_type=0x%x", ticket, phase, u32(readStatus), u32(format), u32(pixelType));
        // R32F renderability alone does not establish a legal readPixels pair.
        // Be conservative: only use the implementation-reported float pair.
        if (readQueryClean && readStatus == GL_FRAMEBUFFER_COMPLETE &&
            (format == GL_RED || format == GL_RGBA) && pixelType == GL_FLOAT)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            packChanged = true;
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ROW_LENGTH, 0);
            glPixelStorei(GL_PACK_SKIP_ROWS, 0);
            glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
            if (CheckErrors(ticket, phase, "pack_setup"))
            {
                GLfloat value[4]{};
                glReadPixels(0, 0, 1, 1, GLenum(format), GL_FLOAT, value);
                if (CheckErrors(ticket, phase, "read_pixels"))
                {
                    if (format == GL_RED)
                    {
                        Msg("* [gles_angle_audit] luminance ticket=%u phase=%s exposure=%.9g read_format=0x%x "
                            "components=1 finite=%u", ticket, phase, double(value[0]), u32(format),
                            u32(std::isfinite(value[0])));
                    }
                    else
                    {
                        Msg("* [gles_angle_audit] luminance ticket=%u phase=%s exposure=%.9g read_format=0x%x "
                            "components=4 rgba=(%.9g,%.9g,%.9g,%.9g) finite=%u", ticket, phase, double(value[0]),
                            u32(format), double(value[0]), double(value[1]), double(value[2]), double(value[3]),
                            u32(std::isfinite(value[0])));
                    }
                }
            }
        }
        else
        {
            Msg("! [gles_angle_audit] luminance ticket=%u phase=%s readback=unsupported reason=implementation_float_pair",
                ticket, phase);
        }
    }

    if (packChanged)
    {
        for (u32 i = 0; i < 4; ++i)
            glPixelStorei(packNames[i], pack[i]);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(packBuffer));
    }
    // READ_BUFFER belongs to the FBO: restore the target before switching back
    // so a different original READ_FBO's read-buffer selection is never changed.
    if (readBufferChanged)
        glReadBuffer(GLenum(targetReadBuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(readFbo));
    bool restored = CheckErrors(ticket, phase, "restore");
    const auto verify = [&](GLenum field, GLint expected)
    {
        GLint actual = 0;
        glGetIntegerv(field, &actual);
        if (actual != expected)
        {
            restored = false;
            Msg("! [gles_angle_audit] luminance ticket=%u phase=%s restore_mismatch=0x%x expected=%d actual=%d",
                ticket, phase, u32(field), expected, actual);
        }
    };
    verify(GL_DRAW_FRAMEBUFFER_BINDING, drawFbo);
    verify(GL_READ_FRAMEBUFFER_BINDING, readFbo);
    verify(GL_READ_BUFFER, oldReadBuffer);
    verify(GL_PIXEL_PACK_BUFFER_BINDING, packBuffer);
    for (u32 i = 0; i < 4; ++i)
        verify(packNames[i], pack[i]);
    restored = CheckErrors(ticket, phase, "restore_queries") && restored;
    Msg("* [gles_angle_audit] luminance ticket=%u phase=%s restored=%u", ticket, phase, u32(restored));
}
} // namespace angle_audit
#endif
}

void CRenderTarget::phase_luminance()
{
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope luminanceScope(GLGpuProfiler::Stage::Luminance);
#endif
    using namespace phase_luminance;

    u32 Offset = 0;
#ifdef USE_DX9 // XXX: check why eps is 0 for other renderers
    float eps = EPS_S;
#else
    float eps = 0;
#endif

    // Targets
    RCache.set_Stencil(FALSE);
    RCache.set_CullMode(CULL_NONE);
    RCache.set_ColorWriteEnable();
    RCache.set_Z(false);

    // 000: Perform LUM-SAT, pass 0, 256x256 => 64x64
    u_setrt(RCache, rt_LUM_64, 0, 0, 0);
    {
        float ts = 64;
        float _w = float(BLOOM_size_X);
        float _h = float(BLOOM_size_Y);
        Fvector2 one = {2.f / _w, 2.f / _h}; // two, infact
        Fvector2 half = {1.f / _w, 1.f / _h}; // one, infact
        Fvector2 a_0 = {half.x + 0, half.y + 0};
        Fvector2 a_1 = {half.x + one.x, half.y + 0};
        Fvector2 a_2 = {half.x + 0, half.y + one.y};
        Fvector2 a_3 = {half.x + one.x, half.y + one.y};
        Fvector2 b_0 = {1 + a_0.x, 1 + a_0.y};
        Fvector2 b_1 = {1 + a_1.x, 1 + a_1.y};
        Fvector2 b_2 = {1 + a_2.x, 1 + a_2.y};
        Fvector2 b_3 = {1 + a_3.x, 1 + a_3.y};

        // Fill vertex buffer
        v_build* pv = (v_build*)RImplementation.Vertex.Lock(4, g_bloom_build->vb_stride, Offset);

#if defined(USE_DX11)
        pv->p.set(eps, float(ts + eps), eps, 1.f);
        pv->uv0.set(a_0.x, b_0.y);
        pv->uv1.set(a_1.x, b_1.y);
        pv->uv2.set(a_2.x, b_2.y);
        pv->uv3.set(a_3.x, b_3.y);
        pv++;
        pv->p.set(eps, eps, eps, 1.f);
        pv->uv0.set(a_0.x, a_0.y);
        pv->uv1.set(a_1.x, a_1.y);
        pv->uv2.set(a_2.x, a_2.y);
        pv->uv3.set(a_3.x, a_3.y);
        pv++;
        pv->p.set(float(ts + eps), float(ts + eps), eps, 1.f);
        pv->uv0.set(b_0.x, b_0.y);
        pv->uv1.set(b_1.x, b_1.y);
        pv->uv2.set(b_2.x, b_2.y);
        pv->uv3.set(b_3.x, b_3.y);
        pv++;
        pv->p.set(float(ts + eps), eps, eps, 1.f);
        pv->uv0.set(b_0.x, a_0.y);
        pv->uv1.set(b_1.x, a_1.y);
        pv->uv2.set(b_2.x, a_2.y);
        pv->uv3.set(b_3.x, a_3.y);
        pv++;
#elif defined(USE_OGL)
        pv->p.set(eps, eps, eps, 1.f);
        pv->uv0.set(a_0.x, a_0.y);
        pv->uv1.set(a_1.x, a_1.y);
        pv->uv2.set(a_2.x, a_2.y);
        pv->uv3.set(a_3.x, a_3.y);
        pv++;
        pv->p.set(eps, float(ts + eps), eps, 1.f);
        pv->uv0.set(a_0.x, b_0.y);
        pv->uv1.set(a_1.x, b_1.y);
        pv->uv2.set(a_2.x, b_2.y);
        pv->uv3.set(a_3.x, b_3.y);
        pv++;
        pv->p.set(float(ts + eps), eps, eps, 1.f);
        pv->uv0.set(b_0.x, a_0.y);
        pv->uv1.set(b_1.x, a_1.y);
        pv->uv2.set(b_2.x, a_2.y);
        pv->uv3.set(b_3.x, a_3.y);
        pv++;
        pv->p.set(float(ts + eps), float(ts + eps), eps, 1.f);
        pv->uv0.set(b_0.x, b_0.y);
        pv->uv1.set(b_1.x, b_1.y);
        pv->uv2.set(b_2.x, b_2.y);
        pv->uv3.set(b_3.x, b_3.y);
        pv++;
#else
#   error No graphics API selected or enabled!
#endif
        RImplementation.Vertex.Unlock(4, g_bloom_build->vb_stride);
        RCache.set_Element(s_luminance->E[0]);
        RCache.set_Geometry(g_bloom_build);
        RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
    }

    // 111: Perform LUM-SAT, pass 1, 64x64 => 8x8
    u_setrt(RCache, rt_LUM_8, 0, 0, 0);
    {
        // Build filter-kernel
        float _ts = 8;
        float _src = float(64);
        Fvector2 a[16], b[16];
        for (int k = 0; k < 16; k++)
        {
            int _x = (k * 2 + 1) % 8; // 1,3,5,7
            int _y = ((k / 4) * 2 + 1); // 1,1,1,1 ~ 3,3,3,3 ~...etc...
            a[k].set(_x, _y).div(_src);
            b[k].set(a[k]).add(1);
        }

        // Fill vertex buffer
        v_filter* pv = (v_filter*)RImplementation.Vertex.Lock(4, g_bloom_filter->vb_stride, Offset);
#if defined(USE_DX11)
        pv->p.set(eps, float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, b[t].y, b[t + 8].y, a[t + 8].x); // xy/yx	- left+down
        pv++;
        pv->p.set(eps, eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, a[t].y, a[t + 8].y, a[t + 8].x); // xy/yx	- left+up
        pv++;
        pv->p.set(float(_ts + eps), float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, b[t].y, b[t + 8].y, b[t + 8].x); // xy/yx	- right+down
        pv++;
        pv->p.set(float(_ts + eps), eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, a[t].y, a[t + 8].y, b[t + 8].x); // xy/yx	- right+up
        pv++;
#elif defined(USE_OGL)
        pv->p.set(eps, eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, a[t].y, a[t + 8].y, a[t + 8].x); // xy/yx	- left+up
        pv++;
        pv->p.set(eps, float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, b[t].y, b[t + 8].y, a[t + 8].x); // xy/yx	- left+down
        pv++;
        pv->p.set(float(_ts + eps), eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, a[t].y, a[t + 8].y, b[t + 8].x); // xy/yx	- right+up
        pv++;
        pv->p.set(float(_ts + eps), float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, b[t].y, b[t + 8].y, b[t + 8].x); // xy/yx	- right+down
        pv++;
#else
#   error No graphics API selected or enabled!
#endif
        RImplementation.Vertex.Unlock(4, g_bloom_filter->vb_stride);
        RCache.set_Element(s_luminance->E[1]);
        RCache.set_Geometry(g_bloom_filter);
        RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
    }

    // 222: Perform LUM-SAT, pass 2, 8x8 => 1x1
    u32 gpu_id = Device.dwFrame % HW.Caps.iGPUNum;
    u_setrt(RCache, rt_LUM_pool[gpu_id * 2 + 1], 0, 0, 0);
    {
        // Build filter-kernel
        float _ts = 1;
        float _src = float(8);
        Fvector2 a[16], b[16];
        for (int k = 0; k < 16; k++)
        {
            int _x = (k * 2 + 1) % 8; // 1,3,5,7
            int _y = ((k / 4) * 2 + 1); // 1,1,1,1 ~ 3,3,3,3 ~...etc...
            a[k].set(_x, _y).div(_src);
            b[k].set(a[k]).add(1);
        }

        // Fill vertex buffer
        v_filter* pv = (v_filter*)RImplementation.Vertex.Lock(4, g_bloom_filter->vb_stride, Offset);
#if defined(USE_DX11)
        pv->p.set(eps, float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, b[t].y, b[t + 8].y, a[t + 8].x); // xy/yx	- left+down
        pv++;
        pv->p.set(eps, eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, a[t].y, a[t + 8].y, a[t + 8].x); // xy/yx	- left+up
        pv++;
        pv->p.set(float(_ts + eps), float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, b[t].y, b[t + 8].y, b[t + 8].x); // xy/yx	- right+down
        pv++;
        pv->p.set(float(_ts + eps), eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, a[t].y, a[t + 8].y, b[t + 8].x); // xy/yx	- right+up
        pv++;
#elif defined(USE_OGL)
        pv->p.set(eps, eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, a[t].y, a[t + 8].y, a[t + 8].x); // xy/yx	- left+up
        pv++;
        pv->p.set(eps, float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(a[t].x, b[t].y, b[t + 8].y, a[t + 8].x); // xy/yx	- left+down
        pv++;
        pv->p.set(float(_ts + eps), eps, eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, a[t].y, a[t + 8].y, b[t + 8].x); // xy/yx	- right+up
        pv++;
        pv->p.set(float(_ts + eps), float(_ts + eps), eps, 1.f);
        for (int t = 0; t < 8; t++)
            pv->uv[t].set(b[t].x, b[t].y, b[t + 8].y, b[t + 8].x); // xy/yx	- right+down
        pv++;
#else
#   error No graphics API selected or enabled!
#endif
        RImplementation.Vertex.Unlock(4, g_bloom_filter->vb_stride);

        f_luminance_adapt = .9f * f_luminance_adapt + .1f * Device.fTimeDelta * ps_r2_tonemap_adaptation;
        float amount = ps_r2_ls_flags.test(R2FLAG_TONEMAP) ? ps_r2_tonemap_amount : 0;
        Fvector3 _none, _full, _result;
        _none.set(1, 0, 1);
        _full.set(ps_r2_tonemap_middlegray, 1.f, ps_r2_tonemap_low_lum);
        _result.lerp(_none, _full, amount);

        RCache.set_Element(s_luminance->E[2]);
        RCache.set_Geometry(g_bloom_filter);
        RCache.set_c("MiddleGray", _result.x, _result.y, _result.z, f_luminance_adapt);
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
        const u32 auditTicket = angle_audit::NextSample(gpu_id);
        if (auditTicket)
        {
            Msg("* [gles_angle_audit] luminance ticket=%u tonemap=%u amount=%.9g middlegray=(%.9g,%.9g,%.9g) adapt=%.9g dt=%.9g",
                auditTicket, u32(ps_r2_ls_flags.test(R2FLAG_TONEMAP)), double(amount), double(_result.x),
                double(_result.y), double(_result.z), double(f_luminance_adapt), double(Device.fTimeDelta));
            angle_audit::Probe(auditTicket, gpu_id, "before", *rt_LUM_pool[gpu_id * 2 + 1]);
        }
        const bool lightingAudit = gles_lighting_audit_current_frame();
        if (lightingAudit)
        {
            Msg("* [gles_lighting_audit] frame=%u exposure_ticket=%u tonemap=%u middlegray=%g amount=%g low_lum=%g adapt=%g",
                Device.dwFrame, Device.dwFrame, u32(ps_r2_ls_flags.test(R2FLAG_TONEMAP)),
                ps_r2_tonemap_middlegray, amount, ps_r2_tonemap_low_lum, f_luminance_adapt);
            angle_audit::Probe(Device.dwFrame, gpu_id, "lighting_used", *rt_LUM_pool[gpu_id * 2 + 1]);
        }
#endif
        RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
        if (auditTicket)
            angle_audit::Probe(auditTicket, gpu_id, "after", *rt_LUM_pool[gpu_id * 2 + 1]);
        if (lightingAudit)
            angle_audit::Probe(Device.dwFrame, gpu_id, "lighting_next", *rt_LUM_pool[gpu_id * 2 + 1]);
#endif
    }

    // Cleanup states
    RCache.set_Z(true);
}
} // namespace xray::render::RENDER_NAMESPACE
