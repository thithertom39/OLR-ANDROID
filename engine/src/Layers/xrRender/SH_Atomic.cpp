#include "stdafx.h"
#pragma hdrstop

#include "SH_Atomic.h"
#include "ResourceManager.h"
#if defined(USE_OGL)
#include "Layers/xrRenderGL/glBackendStateCache.h"
#endif

namespace xray::render::RENDER_NAMESPACE
{
#if defined(USE_OGL)
namespace
{
void DeleteOwnedGLStage(GLuint handle, pcstr name)
{
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
    if (Core.Params && strstr(Core.Params, "-gles_shader_delete_audit"))
    {
        static unsigned zeros = 0, invalid = 0;
        if (!handle && zeros++ == 0)
            Msg("* [gles_shader_delete] ignored empty stage '%s'", name);
        if (handle && !GLAD_GL_ARB_separate_shader_objects && !glIsShader(handle) && invalid++ < 8)
            Msg("! [gles_shader_delete] invalid stage='%s' handle=%u is_program=%u", name, handle, unsigned(glIsProgram(handle)));
    }
#endif
    // Empty/null stages own no object. Avoid dispatching a deletion for them.
    if (!handle)
        return;
    if (GLAD_GL_ARB_separate_shader_objects)
        CHK_GL(glDeleteProgram(handle));
    else
        CHK_GL(glDeleteShader(handle));
}
}
#endif
// Atomic
//SVS::~SVS()
//{
//    _RELEASE(vs);
//    dxRenderDeviceRender::Instance().Resources->_DeleteVS(this);
//}
//SPS::~SPS()
//{
//     _RELEASE(ps);
//     dxRenderDeviceRender::Instance().Resources->_DeletePS(this);
//}
//SState::~SState()
//{
//    _RELEASE(state);
//    dxRenderDeviceRender::Instance().Resources->_DeleteState(this);
//}
//SDeclaration::~SDeclaration()
//{
//    _RELEASE(dcl);
//    dxRenderDeviceRender::Instance().Resources->_DeleteDecl(this);
//}

///////////////////////////////////////////////////////////////////////
//  SVS
SVS::SVS() : sh(0)
#if defined(USE_DX11)
//  ,signature(0)
#endif
{}

SVS::~SVS()
{
    RImplementation.Resources->_DeleteVS(this);

#if defined(USE_DX11)
    // XXX: check just in case
    //_RELEASE(signature);
    //	Now it is release automatically
#endif

#if defined(USE_DX11)
    _RELEASE(sh);
#elif defined(USE_OGL)
    DeleteOwnedGLStage(sh, cName.c_str());
#else
#   error No graphics API selected or enabled!
#endif
}

///////////////////////////////////////////////////////////////////////
// SPS
SPS::~SPS()
{
#if defined(USE_DX11)
    _RELEASE(sh);
#elif defined(USE_OGL)
    DeleteOwnedGLStage(sh, cName.c_str());
#else
#   error No graphics API selected or enabled!
#endif

    RImplementation.Resources->_DeletePS(this);
}

///////////////////////////////////////////////////////////////////////
// SGS
SGS::~SGS()
{
#   if defined(USE_DX11)
    _RELEASE(sh);
#   elif defined(USE_OGL)
    DeleteOwnedGLStage(sh, cName.c_str());
#   else
#       error No graphics API selected or enabled!
#   endif

    RImplementation.Resources->_DeleteGS(this);
}

SHS::~SHS()
{
#   if defined(USE_DX11)
    _RELEASE(sh);
#   elif defined(USE_OGL)
    DeleteOwnedGLStage(sh, cName.c_str());
#   else
#       error No graphics API selected or enabled!
#   endif

    RImplementation.Resources->_DeleteHS(this);
}

SDS::~SDS()
{
#   if defined(USE_DX11)
    _RELEASE(sh);
#   elif defined(USE_OGL)
    DeleteOwnedGLStage(sh, cName.c_str());
#   endif

    RImplementation.Resources->_DeleteDS(this);
}

SCS::~SCS()
{
#    if defined(USE_DX11)
    _RELEASE(sh);
#    elif defined(USE_OGL)
    DeleteOwnedGLStage(sh, cName.c_str());
#   else
#       error No graphics API selected or enabled!
#   endif

    RImplementation.Resources->_DeleteCS(this);
}

#if defined(USE_OGL)
SPP::~SPP()
{
    GLHardwareState::Get().program.invalidate();
    if (GLAD_GL_ARB_separate_shader_objects)
        CHK_GL(glDeleteProgramPipelines(1, &pp));
    else
        CHK_GL(glDeleteProgram(pp));

    RImplementation.Resources->_DeletePP(this);
}
#endif // USE_OGL


#if defined(USE_DX11)
///////////////////////////////////////////////////////////////////////
//	SInputSignature
SInputSignature::SInputSignature(ID3DBlob* pBlob)
{
    VERIFY(pBlob);
    signature = pBlob;
    signature->AddRef();
};

SInputSignature::~SInputSignature()
{
    _RELEASE(signature);
    RImplementation.Resources->_DeleteInputSignature(this);
}
#endif // USE_DX11

///////////////////////////////////////////////////////////////////////
//	SState
SState::~SState()
{
    _RELEASE(state);
    RImplementation.Resources->_DeleteState(this);
}

///////////////////////////////////////////////////////////////////////
//	SDeclaration
SDeclaration::~SDeclaration()
{
    RImplementation.Resources->_DeleteDecl(this);
    //	Release vertex layout
#if defined(USE_DX11)
    xr_map<ID3DBlob*, ID3DInputLayout*>::iterator iLayout;
    iLayout = vs_to_layout.begin();
    for (; iLayout != vs_to_layout.end(); ++iLayout)
    {
        //	Release vertex layout
        _RELEASE(iLayout->second);
    }
#elif defined(USE_OGL)
    glDeleteVertexArrays(1, &dcl);
    GLHardwareState::InvalidateGeometry();
#else
#   error No graphics API selected or enabled!
#endif
}
} // namespace xray::render::RENDER_NAMESPACE
