#pragma once

#include "glStateUtils.h"
#include "glBackendStateCache.h"
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "glGpuProfiler.h"
#endif
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
#include "glSpotWriterAudit.h"
#endif
#if defined(XR_PLATFORM_ANDROID)
#include "xrEngine/AndroidSceneCompare.h"
#endif

namespace xray::render::RENDER_NAMESPACE
{
IC void CBackend::set_xform(u32 ID, const Fmatrix& M)
{
    stat.xforms++;
    //	TODO: OGL: Implement CBackend::set_xform
    //VERIFY(!"Implement CBackend::set_xform");
}

IC GLuint CBackend::get_FB()
{
    return pFB;
}

IC void CBackend::set_FB(GLuint FB)
{
    pFB = FB;
    if (GLHardwareState::Get().framebuffer.update(FB, gles_state_rebind_enabled()))
    {
        PGO(Msg("PGO:set_FB"));
        CHK_GL(glBindFramebuffer(GL_FRAMEBUFFER, pFB));
        GLHardwareState::InvalidateDrawBuffers();
        GLHardwareState::InvalidateAttachments();
    }
}

IC void CBackend::set_RT(GLuint RT, u32 ID)
{
    VERIFY(ID < 4);
    set_FB(pFB ? pFB : HW.pFB);
    pRT[ID] = RT;
    if (GLHardwareState::Get().color_attachments[ID].update(RT, gles_state_rebind_enabled()))
    {
        PGO(Msg("PGO:setRT"));
        stat.target_rt++;
        // TODO: OGL: Implement support for multi-sampled render targets
        CHK_GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + ID, GL_TEXTURE_2D, RT, 0));
    }
}

IC void CBackend::set_ZB(GLuint ZB)
{
    set_FB(pFB ? pFB : HW.pFB);
    pZB = ZB;
    if (GLHardwareState::Get().depth_attachment.update(ZB, gles_state_rebind_enabled()))
    {
        PGO(Msg("PGO:setZB"));
        stat.target_zb++;
        // TODO: OGL: Implement support for multi-sampled render targets
        CHK_GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, ZB, 0));
    }
}

IC void CBackend::ClearRT(GLuint rt, const Fcolor& color)
{
    // TODO: OGL: Implement support for multi-sampled render targets
    set_RT(rt);
    set_ColorWriteEnable();
    glClearColor(color.r, color.g, color.b, color.a);

    CHK_GL(glClear(GL_COLOR_BUFFER_BIT));
}

IC void CBackend::ClearZB(GLuint zb, float depth)
{
    VERIFY(pZB == zb); // do not allow to clear unbound depth
    // TODO: OGL: Implement support for multi-sampled render targets
    set_ZB(zb);

    GLHardwareState::SetDepthMask(true, gles_state_rebind_enabled());
    glClearDepthf(depth);

    CHK_GL(glClear(GL_DEPTH_BUFFER_BIT));
}

IC void CBackend::ClearZB(GLuint zb, float depth, u8 stencil)
{
    VERIFY(pZB == zb); // do not allow to clear unbound depth
    // TODO: OGL: Implement support for multi-sampled render targets
    set_ZB(zb);

    GLHardwareState::SetDepthMask(true, gles_state_rebind_enabled());
    glClearDepthf(depth);

    glStencilMask(~0);
    GLHardwareState::Get().stencil.invalidate();
    glClearStencil(stencil);

    CHK_GL(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
}

IC bool CBackend::ClearRTRect(GLuint rt, const Fcolor& color, size_t numRects, const Irect* rects)
{
    // TODO: OGL: Implement support for multi-sampled render targets
    set_RT(rt);

    CHK_GL(glEnable(GL_SCISSOR_TEST));

    for (size_t i = 0; i < numRects; ++i, ++rects)
    {
        // The window space is inverted compared to DX
        // so we have to invert our vertical coordinates
        const u32 bottom = Device.dwHeight - rects->bottom;

        // The origin of the scissor box is lower-left
        CHK_GL(glScissor(rects->left, bottom, rects->width(), rects->height()));

        // Clear the color buffer without affecting the global state
        set_ColorWriteEnable();
        glClearColor(color.r, color.g, color.b, color.a);

        CHK_GL(glClear(GL_COLOR_BUFFER_BIT));
    }

    CHK_GL(glDisable(GL_SCISSOR_TEST));

    return true;
}

IC bool CBackend::ClearZBRect(GLuint zb, float depth, size_t numRects, const Irect* rects)
{
    // TODO: OGL: Implement support for multi-sampled render targets
    set_ZB(zb);

    CHK_GL(glEnable(GL_SCISSOR_TEST));

    for (size_t i = 0; i < numRects; ++i, ++rects)
    {
        // The window space is inverted compared to DX
        // so we have to invert our vertical coordinates
        const u32 bottom = Device.dwHeight - rects->bottom;

        // The origin of the scissor box is lower-left
        CHK_GL(glScissor(rects->left, bottom, rects->width(), rects->height()));

        GLHardwareState::SetDepthMask(true, gles_state_rebind_enabled());
        glClearDepthf(depth);

        CHK_GL(glClear(GL_DEPTH_BUFFER_BIT));
    }

    CHK_GL(glDisable(GL_SCISSOR_TEST));

    return true;
}

ICF void CBackend::set_Format(SDeclaration* _decl)
{
    if (!_decl)
    {
        decl = nullptr;
        vb = 0;
        vb_stride = 0;
        ib = 0;
        cached_baseV = 0;
        cached_bind_vb = 0;
        cached_bind_stride = 0;
        cached_bind_offset = size_t(-1);
        return;
    }

    const bool declaration_changed = decl != _decl;
    const bool hardware_changed = GLHardwareState::Get().vertex_array.update(_decl->dcl, gles_state_rebind_enabled());
    if (hardware_changed || declaration_changed)
    {
        PGO(Msg("PGO:v_format:%x", _decl));
#ifdef DEBUG
		stat.decl++;
#endif
        decl = _decl;
        if (hardware_changed)
        {
            CHK_GL(glBindVertexArray(_decl->dcl));
            GLHardwareState::Get().vertex_valid = false;
            GLHardwareState::Get().index_buffer.invalidate();
        }

        if (!declaration_changed)
            return; // Rebinding a VAO must preserve this command list's logical buffers.

        // Clear cached vertex and index buffers because buffer bindings are stored inside VAO!
        vb = 0;
        vb_stride = 0;
        ib = 0;
        cached_baseV = 0;
        cached_bind_vb = 0;
        cached_bind_stride = 0;
        cached_bind_offset = size_t(-1);
    }
}

ICF void CBackend::set_PS(GLuint _ps, LPCSTR _n)
{
    if (ps != _ps)
    {
#ifdef RBackend_PGO
		string_path name;
#endif
        PGO(glGetObjectLabel(GL_PROGRAM, _ps, sizeof(name), nullptr, name));
        PGO(Msg("PGO:Pshader:%d,%s", _ps, _n ? _n : name));
        stat.ps++;
        ps = _ps;
        //CHK_GL(glUseProgramStages(HW.pPP, GL_FRAGMENT_SHADER_BIT, ps));
#ifdef DEBUG
		ps_name = _n;
#endif
    }
}

ICF void CBackend::set_GS(GLuint _gs, LPCSTR _n)
{
    if (gs != _gs)
    {
#ifdef RBackend_PGO
		string_path name;
#endif
        PGO(glGetObjectLabel(GL_PROGRAM, _gs, sizeof(name), nullptr, name));
        PGO(Msg("PGO:Gshader:%d,%s", _gs, _n ? _n : name));
        stat.gs++;
        gs = _gs;
        //CHK_GL(glUseProgramStages(HW.pPP, GL_GEOMETRY_SHADER_BIT, gs));
#ifdef DEBUG
		gs_name = _n;
#endif
    }
}

ICF void CBackend::set_VS(GLuint _vs, LPCSTR _n)
{
    if (vs != _vs)
    {
#ifdef RBackend_PGO
		string_path name;
#endif
        PGO(glGetObjectLabel(GL_PROGRAM, _vs, sizeof(name), nullptr, name));
        PGO(Msg("PGO:Vshader:%d,%s", _vs, _n ? _n : name));
        stat.vs++;
        vs = _vs;
        //CHK_GL(glUseProgramStages(HW.pPP, GL_VERTEX_SHADER_BIT, vs));
#ifdef DEBUG
		vs_name = _n;
#endif
    }
}

ICF void CBackend::set_PP(GLuint _pp, pcstr _n)
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    GLSpotWriterAudit::NamePP(context_id, _n);
#endif
    pp = _pp;
    if (GLHardwareState::Get().program.update(_pp, gles_state_rebind_enabled()))
    {
#ifdef RBackend_PGO
        string_path name;
        if (GLAD_GL_ARB_separate_shader_objects)
            glGetObjectLabel(GL_PROGRAM_PIPELINE, _pp, sizeof(name), nullptr, name)
        else
            glGetObjectLabel(GL_PROGRAM, _pp, sizeof(name), nullptr, name)
#endif
        PGO(Msg("PGO:PPshader:%d,%s", _pp, _n ? _n : name));
        stat.pp++;
        if (GLAD_GL_ARB_separate_shader_objects)
            CHK_GL(glBindProgramPipeline(pp));
        else
            CHK_GL(glUseProgram(pp));
#ifdef DEBUG
        pp_name = _n;
#endif
    }
}

ICF void CBackend::bindVertexBase(u32 baseV)
{
    if (!decl)
        return;
    set_Format(decl);
    auto& hardware = GLHardwareState::Get();
    u32 stride = vb_stride;
    if (!stride && decl)
        stride = GetDeclVertexSize(decl->dcl_code.data(), 0);
    const size_t byteOffset = size_t(baseV) * size_t(stride);
    if (!gles_state_rebind_enabled() && hardware.vertex_valid && hardware.vertex_buffer == vb &&
        hardware.vertex_stride == stride && hardware.vertex_offset == byteOffset)
        return;
    if (GLAD_GL_ARB_vertex_attrib_binding)
    {
        CHK_GL(glBindVertexBuffer(0, vb, byteOffset, stride));
    }
    else
    {
        CHK_GL(glBindBuffer(GL_ARRAY_BUFFER, vb));
        SetGLVertexPointer(decl, byteOffset);
    }
    cached_bind_vb = vb;
    cached_bind_stride = stride;
    cached_bind_offset = byteOffset;
    cached_baseV = baseV;
    hardware.vertex_buffer = vb;
    hardware.vertex_stride = stride;
    hardware.vertex_offset = byteOffset;
    hardware.vertex_valid = true;
}

ICF void CBackend::set_Vertices(GLuint _vb, u32 _vb_stride)
{
    {
        PGO(Msg("PGO:VB:%x,%d", _vb, _vb_stride));
#ifdef DEBUG
		stat.vb++;
#endif
        vb = _vb;
        vb_stride = _vb_stride;

        bindVertexBase(0);
    }
}

ICF void CBackend::set_Indices(GLuint _ib)
{
    if (decl)
        set_Format(decl);
    ib = _ib;
    if (GLHardwareState::Get().index_buffer.update(_ib, gles_state_rebind_enabled()))
    {
        PGO(Msg("PGO:IB:%x", _ib));
#ifdef DEBUG
		stat.ib++;
#endif
        CHK_GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib));
    }
}

IC GLenum TranslateTopology(D3DPRIMITIVETYPE T)
{
    static GLenum translateTable[] =
    {
        GL_NONE, //	None
        GL_POINTS, //	D3DPT_POINTLIST = 1,
        GL_LINES, //	D3DPT_LINELIST = 2,
        GL_LINE_STRIP, //	D3DPT_LINESTRIP = 3,
        GL_TRIANGLES, //	D3DPT_TRIANGLELIST = 4,
        GL_TRIANGLE_STRIP, //	D3DPT_TRIANGLESTRIP = 5,
        GL_TRIANGLE_FAN, //	D3DPT_TRIANGLEFAN = 6,
    };

    VERIFY(T<sizeof(translateTable) / sizeof(translateTable[0]));
    VERIFY(T >= 0);

    const GLenum result = translateTable[T];

    VERIFY(result != GL_NONE);

    return result;
}

IC u32 GetIndexCount(D3DPRIMITIVETYPE T, u32 iPrimitiveCount)
{
    switch (T)
    {
    case D3DPT_POINTLIST:
        return iPrimitiveCount;
    case D3DPT_LINELIST:
        return iPrimitiveCount * 2;
    case D3DPT_LINESTRIP:
        return iPrimitiveCount + 1;
    case D3DPT_TRIANGLELIST:
        return iPrimitiveCount * 3;
    case D3DPT_TRIANGLESTRIP:
        return iPrimitiveCount + 2;
    default: NODEFAULT;
#ifdef DEBUG
		return 0;
#endif // #ifdef DEBUG
    }
}

ICF void CBackend::Render(D3DPRIMITIVETYPE T, u32 baseV, u32 startV, u32 countV, u32 startI, u32 PC)
{
    if (!decl || !vb || !ib || PC == 0)
        return;

    GLenum Topology = TranslateTopology(T);
    u32 iIndexCount = GetIndexCount(T, PC);

    stat.render.calls++;
    stat.render.verts += countV;
    stat.render.polys += PC;
    if (pp)
        set_PP(pp);
    set_Format(decl);
    set_Indices(ib);
    constants.flush();

    // Base vertex is applied as a vertex-buffer binding offset (see
    // bindVertexBase), so plain glDrawElements is used on every path.
    // glDrawElementsBaseVertex is deliberately avoided: it segfaults Adreno
    // drivers on flora draws (shared VB, baseV != 0).
    bindVertexBase(baseV);
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    const bool spot_writer_sampled = GLSpotWriterAudit::BeforeDraw(*this, pp, iIndexCount, baseV, startI);
#endif
    CHK_GL(glDrawElements(Topology, iIndexCount, GL_UNSIGNED_SHORT, (void*)(startI * sizeof(GLushort))));
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::RecordDraw();
#endif
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    GLSpotWriterAudit::AfterDraw(spot_writer_sampled);
#endif
    PGO(Msg("PGO:DIP:%dv/%df", countV, PC));
}

ICF void CBackend::Render(D3DPRIMITIVETYPE T, u32 startV, u32 PC)
{
    if (!decl || !vb || PC == 0)
        return;

    GLenum Topology = TranslateTopology(T);
    u32 iIndexCount = GetIndexCount(T, PC);

    stat.render.calls++;
    stat.render.verts += iIndexCount;
    stat.render.polys += PC;
    if (pp)
        set_PP(pp);
    set_Format(decl);
    constants.flush();
    bindVertexBase(0);
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    const bool spot_writer_sampled = GLSpotWriterAudit::BeforeDraw(*this, pp, iIndexCount, 0, startV);
#endif
    CHK_GL(glDrawArrays(Topology, startV, iIndexCount));
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::RecordDraw();
#endif
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    GLSpotWriterAudit::AfterDraw(spot_writer_sampled);
#endif
    PGO(Msg("PGO:DIP:%dv/%df", iIndexCount, PC));
}

IC void CBackend::set_Geometry(SGeometry* _geom)
{
    if (!_geom)
        return;
    set_Format(_geom->dcl._get());

    set_Vertices(_geom->vb, _geom->vb_stride);
    set_Indices(_geom->ib);
}

#if defined(XR_PLATFORM_ANDROID)
ICF void CBackend::set_GeometryForIndexedDraw(SGeometry* geometry)
{
    if (!geometry)
        return;

    static const bool enabled = AndroidSceneCompare::HasOption(Core.Params, "-olr") &&
        !AndroidSceneCompare::HasOption(Core.Params, "-gles_eager_vertex_base");
    if (!enabled)
    {
        set_Geometry(geometry);
        return;
    }

    // Render() binds the real base vertex before drawing. Avoid first binding offset zero
    // on an indexed mesh, then immediately changing the same VAO to another offset.
    set_Format(geometry->dcl._get());
    vb = geometry->vb;
    vb_stride = geometry->vb_stride;
    ib = geometry->ib;
}
#endif

IC void CBackend::set_Scissor(const Irect* R)
{
    if (R)
    {
        CHK_GL(glEnable(GL_SCISSOR_TEST));

        // The window space is inverted compared to DX,
        // so we have to invert our vertical coordinates
        u32 bottom = Device.dwHeight - R->bottom;

        // The origin of the scissor box is lower-left
        CHK_GL(glScissor(R->left, bottom, R->width(), R->height()));
    }
    else
    {
        CHK_GL(glDisable(GL_SCISSOR_TEST));
    }
}

IC void CBackend::SetViewport(const D3D_VIEWPORT& viewport) const
{
    glViewport(viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height);
    glDepthRangef(viewport.MinDepth, viewport.MaxDepth);
}

IC void CBackend::set_Stencil(u32 _enable, u32 _func, u32 _ref, u32 _mask, u32 _writemask, u32 _fail, u32 _pass,
                              u32 _zfail)
{
    const std::array<u32, 8> desired{_enable, _func, _ref, _mask, _writemask, _fail, _pass, _zfail};
    if (!GLHardwareState::Get().stencil.update(desired, gles_state_rebind_enabled()))
        return;
    if (_enable)
    {
        glEnable(GL_STENCIL_TEST);
        CHK_GL(glStencilFunc(glStateUtils::ConvertCmpFunction(_func), _ref, _mask));
        CHK_GL(glStencilMask(_writemask));
        CHK_GL(glStencilOp(glStateUtils::ConvertStencilOp(_fail),
            glStateUtils::ConvertStencilOp(_zfail),
            glStateUtils::ConvertStencilOp(_pass)));
    }
    else
    {
        glDisable(GL_STENCIL_TEST);
    }
}

IC void CBackend::set_Z(u32 _enable)
{
    z_enable = _enable;
    if (GLHardwareState::Get().depth_enable.update(_enable, gles_state_rebind_enabled()))
    {
        if (_enable)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }
}

IC void CBackend::set_ZFunc(u32 _func)
{
    z_func = _func;
    if (GLHardwareState::Get().depth_func.update(_func, gles_state_rebind_enabled()))
    {
        CHK_GL(glDepthFunc(glStateUtils::ConvertCmpFunction(_func)));
    }
}

IC void CBackend::set_AlphaRef(u32 _value)
{
    VERIFY(!"Not implemented.");
}

IC void CBackend::set_ColorWriteEnable(u32 _mask)
{
    colorwrite_mask = _mask;
    if (GLHardwareState::Get().color_mask.update(_mask, gles_state_rebind_enabled()))
    {
        CHK_GL(glColorMask(
            (_mask & D3DCOLORWRITEENABLE_RED) ? GL_TRUE : GL_FALSE,
            (_mask & D3DCOLORWRITEENABLE_GREEN) ? GL_TRUE : GL_FALSE,
            (_mask & D3DCOLORWRITEENABLE_BLUE) ? GL_TRUE : GL_FALSE,
            (_mask & D3DCOLORWRITEENABLE_ALPHA) ? GL_TRUE : GL_FALSE));
    }
}

ICF void CBackend::set_CullMode(u32 _mode)
{
    cull_mode = _mode;
    if (GLHardwareState::Get().cull_mode.update(_mode, gles_state_rebind_enabled()))
    {
        if (_mode == D3DCULL_NONE)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            CHK_GL(glCullFace(glStateUtils::ConvertCullMode(_mode)));
        }
    }
}

ICF void CBackend::set_FillMode(u32 _mode)
{
    fill_mode = _mode;
    if (GLHardwareState::Get().fill_mode.update(_mode, gles_state_rebind_enabled()))
    {
        // glPolygonMode is desktop-only; it is not available in OpenGL ES.
        // Guard against a null function pointer (GLES wireframe fallback is
        // not supported, so we simply keep the last supported fill mode).
        if (glPolygonMode)
            glPolygonMode(GL_FRONT_AND_BACK, glStateUtils::ConvertFillMode(_mode));
    }
}

ICF void CBackend::SetTextureFactor(u32 /*factor*/) const
{
    // Not supported
}

ICF void CBackend::SetAmbient(u32 /*factor*/) const
{
    // Not supported
}

ICF void CBackend::set_VS(ref_vs& _vs)
{
    set_VS(_vs->sh, _vs->cName.c_str());
}

IC void CBackend::set_Constants(R_constant_table* C)
{
    // caching
    if (ctable == C) return;
    ctable = C;
    xforms.unmap();
    hemi.unmap();
    tree.unmap();
    if (nullptr == C) return;

    PGO(Msg("PGO:c-table"));

    // process constant-loaders
    for (auto& Cs : C->table)
        if (Cs->handler) Cs->handler->setup(*this, &*Cs);
}

void CBackend::set_pass_targets(const ref_rt& _1, const ref_rt& _2, const ref_rt& _3, const ref_rt& zb,
    const D3D_VIEWPORT* viewport)
{
    set_FB(HW.pFB);
    if (_1)
    {
        curr_rt_width  = _1->dwWidth;
        curr_rt_height = _1->dwHeight;
    }
    else
    {
        VERIFY(zb);
        curr_rt_width  = zb->dwWidth;
        curr_rt_height = zb->dwHeight;
    }

    const GLenum buffers[3] = {
        (GLenum)(_1 ? GL_COLOR_ATTACHMENT0 : GL_NONE),
        (GLenum)(_2 ? GL_COLOR_ATTACHMENT1 : GL_NONE),
        (GLenum)(_3 ? GL_COLOR_ATTACHMENT2 : GL_NONE)
    };

    set_RT(_1 ? _1->pRT : 0, 0);
    set_RT(_2 ? _2->pRT : 0, 1);
    set_RT(_3 ? _3->pRT : 0, 2);
    set_ZB(zb ? zb->pZRT : 0);

    XR_GL_CHECK_FBO();
    GLHardwareState::SetDrawBuffers(3, buffers, gles_state_rebind_enabled());

    const D3D_VIEWPORT fullViewport = { 0, 0, curr_rt_width, curr_rt_height, 0.f, 1.f };
    SetViewport(viewport ? *viewport : fullViewport);
}
} // namespace xray::render::RENDER_NAMESPACE
