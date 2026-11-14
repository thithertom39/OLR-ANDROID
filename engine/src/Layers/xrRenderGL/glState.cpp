#include "stdafx.h"
#include "glState.h"
#include "glBackendStateCache.h"
#include "glTextureBindings.h"

namespace xray::render::RENDER_NAMESPACE
{
glState::glState()
{
    // Clear the sampler array
    memset(m_samplerArray, 0, CTexture::mtMaxCombinedShaderTextures * sizeof(GLuint));

    rasterizerCullMode = D3DCULL_CCW;

    m_pDepthStencilState.DepthEnable = TRUE;
    m_pDepthStencilState.DepthFunc = D3DCMP_LESSEQUAL;
    m_pDepthStencilState.DepthWriteMask = TRUE;
    m_pDepthStencilState.StencilEnable = TRUE;
    m_pDepthStencilState.StencilFailOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilDepthFailOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilPassOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilFunc = D3DCMP_ALWAYS;
    m_pDepthStencilState.StencilMask = 0xFFFFFFFF;
    m_pDepthStencilState.StencilWriteMask = 0xFFFFFFFF;
    m_pDepthStencilState.StencilRef = 0;

    m_pBlendState.BlendEnable = TRUE;
    m_pBlendState.SrcBlend = D3DBLEND_ONE;
    m_pBlendState.DestBlend = D3DBLEND_ZERO;
    m_pBlendState.SrcBlendAlpha = D3DBLEND_ONE;
    m_pBlendState.DestBlendAlpha = D3DBLEND_ZERO;
    m_pBlendState.BlendOp = D3DBLENDOP_ADD;
    m_pBlendState.BlendOpAlpha = D3DBLENDOP_ADD;
    m_pBlendState.ColorMask = 0xF;

    m_uiMipLODBias = FLT_MAX;
}

glState* glState::Create()
{
    return xr_new<glState>();
}

void glState::Apply(CBackend& cmd_list)
{
    // Sampler bindings belong to the GL context, including the zero bindings
    // omitted by this pass. The shared cache skips bindings that already match.
    const bool force_rebind = CBackend::gles_state_rebind_enabled();
    const u32 sampler_stage_count = GLTextureBindings::SamplerStageCount(m_samplerStageCount, force_rebind);
    for (u32 stage = 0; stage < sampler_stage_count; stage++)
    {
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
        GLTextureBindings::BindSampler(stage, m_samplerArray[stage], force_rebind);
#else
        if (m_samplerArray[stage])
            GLTextureBindings::BindSampler(stage, m_samplerArray[stage], force_rebind);
#endif
        if (m_samplerArray[stage])
        {
            if (!fsimilar(m_uiMipLODBias, ps_r__tf_Mipbias))
            {
                CHK_GL(glSamplerParameterf(m_samplerArray[stage], GL_TEXTURE_MIN_LOD, 0.f));
                CHK_GL(glSamplerParameterf(m_samplerArray[stage], GL_TEXTURE_MAX_LOD, FLT_MAX));
#if !defined(XR_PLATFORM_ANDROID) && !defined(XRAY_USE_GLES)
                CHK_GL(glSamplerParameterf(m_samplerArray[stage], GL_TEXTURE_LOD_BIAS, ps_r__tf_Mipbias));
#endif
            }
        }
    }

    m_uiMipLODBias = ps_r__tf_Mipbias;

    cmd_list.set_CullMode(rasterizerCullMode);
    cmd_list.set_Z(m_pDepthStencilState.DepthEnable);
    cmd_list.set_ZFunc(m_pDepthStencilState.DepthFunc);
    cmd_list.set_Stencil(
        m_pDepthStencilState.StencilEnable,
        m_pDepthStencilState.StencilFunc,
        m_pDepthStencilState.StencilRef,
        m_pDepthStencilState.StencilMask,
        m_pDepthStencilState.StencilWriteMask,
        m_pDepthStencilState.StencilFailOp,
        m_pDepthStencilState.StencilPassOp,
        m_pDepthStencilState.StencilDepthFailOp
    );

    GLHardwareState::SetDepthMask(m_pDepthStencilState.DepthWriteMask != FALSE, force_rebind);

    auto& hardware_state = GLHardwareState::Get();
    const bool blend_enable = m_pBlendState.BlendEnable != FALSE;
    if (hardware_state.blend_enable.update(blend_enable, force_rebind))
    {
        if (blend_enable)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
    }

    const std::array<GLenum, 4> blend_args = {
        glStateUtils::ConvertBlendArg(m_pBlendState.SrcBlend),
        glStateUtils::ConvertBlendArg(m_pBlendState.DestBlend),
        glStateUtils::ConvertBlendArg(m_pBlendState.SrcBlendAlpha),
        glStateUtils::ConvertBlendArg(m_pBlendState.DestBlendAlpha)
    };
    if (hardware_state.blend_args.update(blend_args, force_rebind))
        CHK_GL(glBlendFuncSeparate(blend_args[0], blend_args[1], blend_args[2], blend_args[3]));

    const std::array<GLenum, 2> blend_equations = {
        glStateUtils::ConvertBlendOp(m_pBlendState.BlendOp),
        glStateUtils::ConvertBlendOp(m_pBlendState.BlendOpAlpha)
    };
    if (hardware_state.blend_equations.update(blend_equations, force_rebind))
        CHK_GL(glBlendEquationSeparate(blend_equations[0], blend_equations[1]));

    cmd_list.set_ColorWriteEnable(m_pBlendState.ColorMask);
}

void glState::Release()
{
    // Delete any generated samplers in the array
    GLTextureBindings::DeleteSamplers(CTexture::mtMaxCombinedShaderTextures, m_samplerArray);

    // Clear the sampler array
    memset(m_samplerArray, 0, CTexture::mtMaxCombinedShaderTextures * sizeof(GLuint));
    m_samplerStageCount = 0;

    glState* pState = this;
    xr_delete(pState);
}

void glState::UpdateRenderState(u32 name, u32 value)
{
    switch (name)
    {
    case D3DRS_CULLMODE:
        rasterizerCullMode = (D3DCULL)value;
        break;

    case D3DRS_ZENABLE:
        m_pDepthStencilState.DepthEnable = value ? TRUE : FALSE;
        break;

    case D3DRS_ZWRITEENABLE:
        m_pDepthStencilState.DepthWriteMask = value ? TRUE : FALSE;
        break;

    case D3DRS_ZFUNC:
        m_pDepthStencilState.DepthFunc = (D3DCMPFUNC)value;
        break;

    case D3DRS_STENCILENABLE:
        m_pDepthStencilState.StencilEnable = value ? TRUE : FALSE;
        break;

    case D3DRS_STENCILMASK:
        m_pDepthStencilState.StencilMask = (u32)value;
        break;

    case D3DRS_STENCILWRITEMASK:
        m_pDepthStencilState.StencilWriteMask = (u32)value;
        break;

    case D3DRS_STENCILFAIL:
        m_pDepthStencilState.StencilFailOp = (D3DSTENCILOP)value;
        break;

    case D3DRS_STENCILZFAIL:
        m_pDepthStencilState.StencilDepthFailOp = (D3DSTENCILOP)value;
        break;

    case D3DRS_STENCILPASS:
        m_pDepthStencilState.StencilPassOp = (D3DSTENCILOP)value;
        break;

    case D3DRS_STENCILFUNC:
        m_pDepthStencilState.StencilFunc = (D3DCMPFUNC)value;
        break;

    case D3DRS_STENCILREF:
        m_pDepthStencilState.StencilRef = value;
        break;

    case D3DRS_SRCBLEND:
        m_pBlendState.SrcBlend = (D3DBLEND)value;
        break;

    case D3DRS_DESTBLEND:
        m_pBlendState.DestBlend = (D3DBLEND)value;
        break;

    case D3DRS_BLENDOP:
        m_pBlendState.BlendOp = (D3DBLENDOP)value;
        break;

    case D3DRS_SRCBLENDALPHA:
        m_pBlendState.SrcBlendAlpha = (D3DBLEND)value;
        break;

    case D3DRS_DESTBLENDALPHA:
        m_pBlendState.DestBlendAlpha = (D3DBLEND)value;
        break;

    case D3DRS_BLENDOPALPHA:
        m_pBlendState.BlendOpAlpha = (D3DBLENDOP)value;
        break;

    case D3DRS_ALPHABLENDENABLE:
        m_pBlendState.BlendEnable = value ? TRUE : FALSE;
        break;

    case D3DRS_COLORWRITEENABLE:
    case D3DRS_COLORWRITEENABLE1:
    case D3DRS_COLORWRITEENABLE2:
    case D3DRS_COLORWRITEENABLE3:
        m_pBlendState.ColorMask = (u32)value;
        break;

    case D3DRS_LIGHTING:
    case D3DRS_FOGENABLE:
    case D3DRS_ALPHATESTENABLE:
    case D3DRS_ALPHAREF:
        // Deprecated
        break;

    default:
        VERIFY(!"Render state not implemented");
        break;
    }
}

void glState::UpdateSamplerState(u32 stage, u32 name, u32 value)
{
    // stage is u32, so a negative sentinel (e.g. 0xFFFFFFFF) must not index the array.
    if (stage >= CTexture::mtMaxCombinedShaderTextures)
        return;

    if (m_samplerStageCount <= stage)
        m_samplerStageCount = stage + 1;

    GLint currentFilter = (GLint)GL_NEAREST;

    if (m_samplerArray[stage] == 0)
        glGenSamplers(1, &m_samplerArray[stage]);
    else if (name == D3DSAMP_MINFILTER || name == D3DSAMP_MIPFILTER)
        glGetSamplerParameteriv(m_samplerArray[stage], GL_TEXTURE_MIN_FILTER, &currentFilter);

    switch (name)
    {
    case D3DSAMP_ADDRESSU: /* D3DTEXTUREADDRESS for U coordinate */
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_WRAP_S, glStateUtils::ConvertTextureAddressMode(
            value)));
        break;
    case D3DSAMP_ADDRESSV: /* D3DTEXTUREADDRESS for V coordinate */
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_WRAP_T, glStateUtils::ConvertTextureAddressMode(
            value)));
        break;
    case D3DSAMP_ADDRESSW: /* D3DTEXTUREADDRESS for W coordinate */
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_WRAP_R, glStateUtils::ConvertTextureAddressMode(
            value)));
        break;
    case D3DSAMP_BORDERCOLOR: /* D3DCOLOR */
    {
        // GL_TEXTURE_BORDER_COLOR / glSamplerParameterIuiv are only valid on
        // GLES 3.2+ (desktop GL always supports them). Skip on older ES.
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
        if (!GLAD_GL_ES_VERSION_3_2)
            break;
#endif
        GLuint color[] = {color_get_R(value), color_get_G(value), color_get_B(value), color_get_A(value)};
        CHK_GL(glSamplerParameterIuiv(m_samplerArray[stage], GL_TEXTURE_BORDER_COLOR, color));
    }
        break;
    case D3DSAMP_MAGFILTER: /* D3DTEXTUREFILTER filter to use for magnification */
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_MAG_FILTER, glStateUtils::ConvertTextureFilter(
            value)));
        break;
    case D3DSAMP_MINFILTER: /* D3DTEXTUREFILTER filter to use for minification */
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_MIN_FILTER, glStateUtils::ConvertTextureFilter(
            value, currentFilter)));
        break;
    case D3DSAMP_MIPFILTER: /* D3DTEXTUREFILTER filter to use between mipmaps during minification */
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_MIN_FILTER, glStateUtils::ConvertTextureFilter(
            value, currentFilter, true)));
        break;
    case D3DSAMP_MIPMAPLODBIAS: /* float Mipmap LOD bias */
#if !defined(XR_PLATFORM_ANDROID) && !defined(XRAY_USE_GLES)
        CHK_GL(glSamplerParameterf(m_samplerArray[stage], GL_TEXTURE_LOD_BIAS, value));
#endif
        break;
    case D3DSAMP_MAXMIPLEVEL: /* DWORD 0..(n-1) LOD index of largest map to use (0 == largest) */
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_MAX_LEVEL, value));
        break;
    case D3DSAMP_MAXANISOTROPY: /* DWORD maximum anisotropy */
        if (GLAD_GL_ARB_texture_filter_anisotropic || GLAD_GL_EXT_texture_filter_anisotropic)
            CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_MAX_ANISOTROPY, value));
        break;
    case XRDX11SAMP_COMPARISONFILTER:
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_COMPARE_MODE, value ? (GLint)
            GL_COMPARE_REF_TO_TEXTURE : (GLint)GL_NONE));
        break;
    case XRDX11SAMP_COMPARISONFUNC:
        CHK_GL(glSamplerParameteri(m_samplerArray[stage], GL_TEXTURE_COMPARE_FUNC, value));
        break;
    default:
        VERIFY(!"Sampler state not implemented");
        break;
    }
}
} // namespace xray::render::RENDER_NAMESPACE
