#include "stdafx.h"
#pragma hdrstop

#include "ResourceManager.h"
#include "tss.h"
#include "Blender.h"
#include "Blender_Recorder.h"
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
#include "olr_r1_mode.h"
#endif
#include "xrScriptEngine/script_engine.hpp"
#include "xrScriptEngine/script_space.hpp"
#include "xrCore/Threading/ScopeLock.hpp"

#ifdef DEBUG
#define MDB Memory.dbg_check()
#else
#define MDB
#endif

namespace xray::render::RENDER_NAMESPACE
{
#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
void CResourceManager::CompileOlrR1Native(Shader& S, CBlender_Compile& C, bool vertexLit)
{
    // Original BlenderDefault/Blender_Vertex: lmap[_dt] / vert[_dt].
    // impl[_dt] is the separate implicit-lightmap material, not B_DEFAULT.
    bool diffuseDetail = false, bumpDetail = false;
    C.detail_texture = nullptr;
    C.detail_scaler = nullptr;
    const bool hasDetail = m_textures_description.GetDetailTexture(
        C.L_textures[0], C.detail_texture, C.detail_scaler);
    m_textures_description.GetTextureUsage(C.L_textures[0], diffuseDetail, bumpDetail);
    const bool detail = hasDetail && diffuseDetail && C.detail_texture && C.detail_scaler;
    pcstr vs = vertexLit ? (detail ? "olr_r1_native_vertex_detail" : "olr_r1_native_vertex") :
        (detail ? "olr_r1_native_detail" : "olr_r1_native");
    ShaderElement base;
    C.SH = &base;
    C.RS.Invalidate();
    C.SetParams(S.E[0]->flags.iPriority, false);
    C.r_Pass(vs, vs, FALSE, TRUE, TRUE, FALSE);
    C.r_Sampler("s_base", C.L_textures[0]);
    if (!vertexLit)
    {
        C.r_Sampler_clf("s_lmap", C.L_textures[1].c_str());
        C.r_Sampler_clf("s_hemi", C.L_textures[2].c_str());
    }
    if (detail)
        C.r_Sampler("s_detail", C.detail_texture);
    C.r_End();
    S.olr_r1_lightmap = _CreateElement(std::move(base));

    for (bool spot : {false, true})
    {
        ShaderElement additive;
        C.SH = &additive;
        C.RS.Invalidate();
        C.SetParams(S.E[0]->flags.iPriority, false);
        pcstr dynamic = spot ? "olr_r1_native_spot" : "olr_r1_native_point";
        C.r_Pass(dynamic, dynamic, FALSE, TRUE, FALSE, TRUE, D3DBLEND_ONE, D3DBLEND_ONE);
        C.RS.SetRS(D3DRS_ZFUNC, D3DCMP_EQUAL);
        C.r_ColorWriteEnable(true, true, true, false);
        C.r_Sampler("s_base", C.L_textures[0]);
        C.r_Sampler_clf("s_lmap", spot ? "internal\\internal_light_att" : "internal\\internal_light_attpoint");
        C.r_Sampler_clf("s_r1_att", spot ? "internal\\internal_light_attclip" : "internal\\internal_light_attpoint");
        // add_point / add_spot intentionally have no detail modulation.
        C.r_End();
        if (spot)
            S.olr_r1_lightmap_spot = _CreateElement(std::move(additive));
        else
            S.olr_r1_lightmap_dynamic = _CreateElement(std::move(additive));
    }
    static u32 logged = 0;
    if (logged++ < 8)
        Msg("* OLR R1 native: %s material '%s', detail=%u, original lookup POINT/SPOT, direct UNORM color",
            vertexLit ? "vertex" : "lightmap", C.L_textures[0].c_str(), unsigned(detail));
}
#endif
// wrapper
class adopt_sampler
{
    CBlender_Compile* C;
    u32 stage;

public:
    adopt_sampler(CBlender_Compile* compiler, u32 _stage) : C(compiler), stage(_stage)
    {
        if (u32(-1) == stage)
            C = nullptr;
    }
    adopt_sampler(const adopt_sampler& other) : C(other.C), stage(other.stage)
    {
        if (u32(-1) == stage)
            C = nullptr;
    }

    adopt_sampler& _texture(LPCSTR texture)
    {
        if (C)
            C->i_Texture(stage, texture);
        return *this;
    }
    adopt_sampler& _projective(bool _b)
    {
        if (C)
            C->i_Projective(stage, _b);
        return *this;
    }
    adopt_sampler& _clamp()
    {
        if (C)
            C->i_Address(stage, D3DTADDRESS_CLAMP);
        return *this;
    }
    adopt_sampler& _wrap()
    {
        if (C)
            C->i_Address(stage, D3DTADDRESS_WRAP);
        return *this;
    }
    adopt_sampler& _mirror()
    {
        if (C)
            C->i_Address(stage, D3DTADDRESS_MIRROR);
        return *this;
    }
    adopt_sampler& _f_anisotropic()
    {
        if (C)
            C->i_Filter(stage, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, D3DTEXF_ANISOTROPIC);
        return *this;
    }
    adopt_sampler& _f_trilinear()
    {
        if (C)
            C->i_Filter(stage, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR);
        return *this;
    }
    adopt_sampler& _f_bilinear()
    {
        if (C)
            C->i_Filter(stage, D3DTEXF_LINEAR, D3DTEXF_POINT, D3DTEXF_LINEAR);
        return *this;
    }
    adopt_sampler& _f_linear()
    {
        if (C)
            C->i_Filter(stage, D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTEXF_LINEAR);
        return *this;
    }
    adopt_sampler& _f_none()
    {
        if (C)
            C->i_Filter(stage, D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT);
        return *this;
    }
    adopt_sampler& _fmin_none()
    {
        if (C)
            C->i_Filter_Min(stage, D3DTEXF_NONE);
        return *this;
    }
    adopt_sampler& _fmin_point()
    {
        if (C)
            C->i_Filter_Min(stage, D3DTEXF_POINT);
        return *this;
    }
    adopt_sampler& _fmin_linear()
    {
        if (C)
            C->i_Filter_Min(stage, D3DTEXF_LINEAR);
        return *this;
    }
    adopt_sampler& _fmin_aniso()
    {
        if (C)
            C->i_Filter_Min(stage, D3DTEXF_ANISOTROPIC);
        return *this;
    }
    adopt_sampler& _fmip_none()
    {
        if (C)
            C->i_Filter_Mip(stage, D3DTEXF_NONE);
        return *this;
    }
    adopt_sampler& _fmip_point()
    {
        if (C)
            C->i_Filter_Mip(stage, D3DTEXF_POINT);
        return *this;
    }
    adopt_sampler& _fmip_linear()
    {
        if (C)
            C->i_Filter_Mip(stage, D3DTEXF_LINEAR);
        return *this;
    }
    adopt_sampler& _fmag_none()
    {
        if (C)
            C->i_Filter_Mag(stage, D3DTEXF_NONE);
        return *this;
    }
    adopt_sampler& _fmag_point()
    {
        if (C)
            C->i_Filter_Mag(stage, D3DTEXF_POINT);
        return *this;
    }
    adopt_sampler& _fmag_linear()
    {
        if (C)
            C->i_Filter_Mag(stage, D3DTEXF_LINEAR);
        return *this;
    }
};

// wrapper
class adopt_compiler
{
    CBlender_Compile* C;

public:
    adopt_compiler(CBlender_Compile* compiler) : C(compiler) {}
    adopt_compiler(const adopt_compiler& other) : C(other.C) {}
    adopt_compiler& _options(int P, bool S)
    {
        C->SetParams(P, S);
        return *this;
    }
    adopt_compiler& _o_emissive(bool E)
    {
        C->SH->flags.bEmissive = E;
        return *this;
    }
    adopt_compiler& _o_distort(bool E)
    {
        C->SH->flags.bDistort = E;
        return *this;
    }
    adopt_compiler& _o_wmark(bool E)
    {
        C->SH->flags.bWmark = E;
        return *this;
    }
    adopt_compiler& _pass(LPCSTR vs, LPCSTR ps)
    {
        C->r_Pass(vs, ps, true);
        return *this;
    }
    adopt_compiler& _fog(bool _fog)
    {
        C->PassSET_LightFog(FALSE, _fog);
        return *this;
    }
    adopt_compiler& _zbuffer(bool _test, bool _write)
    {
        C->PassSET_ZB(_test, _write);
        return *this;
    }
    adopt_compiler& _blend(bool _blend, u32 abSRC, u32 abDST)
    {
        C->PassSET_ablend_mode(_blend, abSRC, abDST);
        return *this;
    }
    adopt_compiler& _aref(bool _aref, u32 aref)
    {
        C->PassSET_ablend_aref(_aref, aref);
        return *this;
    }
    adopt_compiler& _color_write_enable(bool cR, bool cG, bool cB, bool cA)
    {
        C->r_ColorWriteEnable(cR, cG, cB, cA);
        return *this;
    }
    adopt_sampler _sampler(LPCSTR _name)
    {
        u32 s = C->r_Sampler(_name, nullptr);
        return adopt_sampler(C, s);
    }
};

class adopt_blend
{
public:
};

// export
void CResourceManager::LS_Load()
{
    // clang-format off
    auto exporterFunc = [](lua_State* luaState)
    {
        using namespace luabind;
        using namespace luabind::policy;

        module(luaState)
        [
            class_<adopt_sampler>("_sampler")
                 .def(constructor<const adopt_sampler&>())
                 .def("texture",       &adopt_sampler::_texture,       return_reference_to<1>())
                 .def("project",       &adopt_sampler::_projective,    return_reference_to<1>())
                 .def("clamp",         &adopt_sampler::_clamp,         return_reference_to<1>())
                 .def("wrap",          &adopt_sampler::_wrap,          return_reference_to<1>())
                 .def("mirror",        &adopt_sampler::_mirror,        return_reference_to<1>())
                 .def("f_anisotropic", &adopt_sampler::_f_anisotropic, return_reference_to<1>())
                 .def("f_trilinear",   &adopt_sampler::_f_trilinear,   return_reference_to<1>())
                 .def("f_bilinear",    &adopt_sampler::_f_bilinear,    return_reference_to<1>())
                 .def("f_linear",      &adopt_sampler::_f_linear,      return_reference_to<1>())
                 .def("f_none",        &adopt_sampler::_f_none,        return_reference_to<1>())
                 .def("fmin_none",     &adopt_sampler::_fmin_none,     return_reference_to<1>())
                 .def("fmin_point",    &adopt_sampler::_fmin_point,    return_reference_to<1>())
                 .def("fmin_linear",   &adopt_sampler::_fmin_linear,   return_reference_to<1>())
                 .def("fmin_aniso",    &adopt_sampler::_fmin_aniso,    return_reference_to<1>())
                 .def("fmip_none",     &adopt_sampler::_fmip_none,     return_reference_to<1>())
                 .def("fmip_point",    &adopt_sampler::_fmip_point,    return_reference_to<1>())
                 .def("fmip_linear",   &adopt_sampler::_fmip_linear,   return_reference_to<1>())
                 .def("fmag_none",     &adopt_sampler::_fmag_none,     return_reference_to<1>())
                 .def("fmag_point",    &adopt_sampler::_fmag_point,    return_reference_to<1>())
                 .def("fmag_linear",   &adopt_sampler::_fmag_linear,   return_reference_to<1>()),

            class_<adopt_compiler>("_compiler")
                .def(constructor<const adopt_compiler&>())
                .def("begin",              &adopt_compiler::_pass,               return_reference_to<1>())
                .def("sorting",            &adopt_compiler::_options,            return_reference_to<1>())
                .def("emissive",           &adopt_compiler::_o_emissive,         return_reference_to<1>())
                .def("distort",            &adopt_compiler::_o_distort,          return_reference_to<1>())
                .def("wmark",              &adopt_compiler::_o_wmark,            return_reference_to<1>())
                .def("fog",                &adopt_compiler::_fog,                return_reference_to<1>())
                .def("zb",                 &adopt_compiler::_zbuffer,                 return_reference_to<1>())
                .def("blend",              &adopt_compiler::_blend,              return_reference_to<1>())
                .def("aref",               &adopt_compiler::_aref,               return_reference_to<1>())
                .def("color_write_enable", &adopt_compiler::_color_write_enable, return_reference_to<1>())
                .def("sampler",            &adopt_compiler::_sampler), // returns sampler-object

            class_<adopt_blend>("blend")
                .enum_("blend")
                [
                    value("zero",         int(D3DBLEND_ZERO)),
                    value("one",          int(D3DBLEND_ONE)),
                    value("srccolor",     int(D3DBLEND_SRCCOLOR)),
                    value("invsrccolor",  int(D3DBLEND_INVSRCCOLOR)),
                    value("srcalpha",     int(D3DBLEND_SRCALPHA)),
                    value("invsrcalpha",  int(D3DBLEND_INVSRCALPHA)),
                    value("destalpha",    int(D3DBLEND_DESTALPHA)),
                    value("invdestalpha", int(D3DBLEND_INVDESTALPHA)),
                    value("destcolor",    int(D3DBLEND_DESTCOLOR)),
                    value("invdestcolor", int(D3DBLEND_INVDESTCOLOR)),
                    value("srcalphasat",  int(D3DBLEND_SRCALPHASAT))
                ]
        ];
    };
    // clang-format on

    ScriptEngine.init(exporterFunc, false);
    // load shaders
    const char* shaderPath = RImplementation.getShaderPath();
    xr_vector<char*>* folder = FS.file_list_open("$game_shaders$", shaderPath, FS_ListFiles | FS_RootOnly);
    VERIFY(folder);
    for (u32 it = 0; it < folder->size(); it++)
    {
        string_path namesp, fn;
        xr_strcpy(namesp, (*folder)[it]);
        if (nullptr == strext(namesp) || 0 != xr_strcmp(strext(namesp), ".s"))
            continue;
        *strext(namesp) = 0;
        if (0 == namesp[0])
            xr_strcpy(namesp, ScriptEngine.GlobalNamespace);
        strconcat(sizeof(fn), fn, shaderPath, (*folder)[it]);
        FS.update_path(fn, "$game_shaders$", fn);
        ScriptEngine.load_file_into_namespace(fn, namesp);
    }
    FS.file_list_close(folder);
}

void CResourceManager::LS_Unload() { ScriptEngine.unload(); }
BOOL CResourceManager::_lua_HasShader(LPCSTR s_shader)
{
    string256 undercorated;
    for (int i = 0, l = xr_strlen(s_shader) + 1; i < l; i++)
        undercorated[i] = (_DELIMITER == s_shader[i]) ? '_' : s_shader[i];

    ScopeLock scope(&ScriptEngineLock);
#ifdef _EDITOR
    return ScriptEngine.object(undercorated, "editor", LUA_TFUNCTION);
#else
    return ScriptEngine.object(undercorated, "normal", LUA_TFUNCTION) ||
        ScriptEngine.object(undercorated, "l_special", LUA_TFUNCTION);
#endif
}

Shader* CResourceManager::_lua_Create(LPCSTR d_shader, LPCSTR s_textures)
{
    CBlender_Compile C;
    Shader S;

    // undecorate
    string256 undercorated;
    for (int i = 0, l = xr_strlen(d_shader) + 1; i < l; i++)
        undercorated[i] = (_DELIMITER == d_shader[i]) ? '_' : d_shader[i];
    LPCSTR s_shader = undercorated;

    // Access to template
    C.BT = nullptr;
    C.bFFP = FALSE;
    C.bDetail = FALSE;

    // Prepare
    _ParseList(C.L_textures, s_textures);
    C.detail_texture = nullptr;
    C.detail_scaler = nullptr;

    ScriptEngineLock.Enter();
    // Compile element (LOD0 - HQ)
    if (ScriptEngine.object(s_shader, "normal_hq", LUA_TFUNCTION))
    {
        // Analyze possibility to detail this shader
        C.iElement = 0;
        C.bDetail = m_textures_description.GetDetailTexture(C.L_textures[0], C.detail_texture, C.detail_scaler);

        if (C.bDetail)
            S.E[0] = C._lua_Compile(s_shader, "normal_hq");
        else
            S.E[0] = C._lua_Compile(s_shader, "normal");
    }
    else
    {
        if (ScriptEngine.object(s_shader, "normal", LUA_TFUNCTION))
        {
            C.iElement = 0;
            C.bDetail = m_textures_description.GetDetailTexture(C.L_textures[0], C.detail_texture, C.detail_scaler);
            S.E[0] = C._lua_Compile(s_shader, "normal");
        }
    }

    // Compile element (LOD1)
    if (ScriptEngine.object(s_shader, "normal", LUA_TFUNCTION))
    {
        C.iElement = 1;
        C.bDetail = m_textures_description.GetDetailTexture(C.L_textures[0], C.detail_texture, C.detail_scaler);
        S.E[1] = C._lua_Compile(s_shader, "normal");
    }

    // Compile element
    if (ScriptEngine.object(s_shader, "l_point", LUA_TFUNCTION))
    {
        C.iElement = 2;
        C.bDetail = FALSE;
        S.E[2] = C._lua_Compile(s_shader, "l_point");
        ;
    }

    // Compile element
    if (ScriptEngine.object(s_shader, "l_spot", LUA_TFUNCTION))
    {
        C.iElement = 3;
        C.bDetail = FALSE;
        S.E[3] = C._lua_Compile(s_shader, "l_spot");
        ;
    }

    // Compile element
    if (ScriptEngine.object(s_shader, "l_special", LUA_TFUNCTION))
    {
        C.iElement = 4;
        C.bDetail = FALSE;
        S.E[4] = C._lua_Compile(s_shader, "l_special");
    }
    ScriptEngineLock.Leave();

#if defined(XR_PLATFORM_ANDROID) && defined(USE_OGL)
    // OLR's level materials normally compile via _lua_Create(), not the
    // C++ blender path. Without this bridge the R1 forward queue stays empty
    // even though -olr_r1_forward is present on the command line.
    // Keep the first stage deliberately limited to known opaque "default"
    // lightmapped materials, never alpha-tested / transparent Lua shaders.
    static const bool olrR1Forward = Core.Params && strstr(Core.Params, "-olr ") &&
        strstr(Core.Params, "-olr_r1_forward");
    const bool nativeVertex = olr_r1_native_enabled() && xr_stricmp(s_shader, "def_vertex") == 0;
    const bool defaultLightmap = xr_stricmp(s_shader, "default") == 0 &&
        C.L_textures.size() == 3 && strncmp(C.L_textures[1].c_str(), "lmap#", 5) == 0 &&
        strncmp(C.L_textures[2].c_str(), "lmap#", 5) == 0;
    if (olrR1Forward && (defaultLightmap || nativeVertex) &&
        S.E[0] &&
        S.E[0]->flags.iPriority < 2 &&
        !S.E[0]->flags.bStrictB2F &&
        !S.E[0]->flags.bEmissive &&
        !S.E[0]->flags.bDistort &&
        !S.E[0]->flags.bWmark)
    {
        if (olr_r1_native_enabled())
            CompileOlrR1Native(S, C, nativeVertex);
        else
        {
        // Build an additional, standalone GLES program; the original Lua
        // shader elements remain available as the deferred fallback.
        // Do not call _cpp_Compile(): there is no C++ blender attached here.
        ShaderElement forward;
        C.SH = &forward;
        C.RS.Invalidate();
        C.SetParams(S.E[0]->flags.iPriority, false);

        C.detail_texture = nullptr;
        C.detail_scaler = nullptr;
        const bool hasDetailTexture = m_textures_description.GetDetailTexture(
            C.L_textures[0], C.detail_texture, C.detail_scaler);
        bool diffuseDetail = false;
        bool bumpDetail = false;
        m_textures_description.GetTextureUsage(C.L_textures[0], diffuseDetail, bumpDetail);
        const bool useDetail = hasDetailTexture && diffuseDetail &&
            C.detail_texture && C.detail_scaler;
        // Experimental original-R1-style lighting: static geometry first,
        // then a separate additive pass for each relevant runtime lamp.
        // Keep the existing single-pass forward mode as a safe fallback.
        const bool multipass = Core.Params && strstr(Core.Params, "-olr_r1_multipass");
        pcstr shader = useDetail ? "olr_r1_lightmap_forward_detail" : "olr_r1_lightmap_forward";
        pcstr pixelShader = multipass
            ? (useDetail ? "olr_r1_lightmap_baked_detail" : "olr_r1_lightmap_baked")
            : shader;
        C.r_Pass(shader, pixelShader, FALSE, TRUE, TRUE, FALSE);
        C.r_Sampler("s_base", C.L_textures[0]);
        C.r_Sampler_clf("s_lmap", C.L_textures[1].c_str());
        C.r_Sampler_clf("s_hemi", C.L_textures[2].c_str());
        // Match the adaptive exposure used by R2's combine_1 for deferred
        // geometry. The forward pass writes into the same LDR scene target.
        C.r_Sampler_rtf("s_tonemap", r2_RT_luminance_cur);
        if (useDetail)
            C.r_Sampler("s_detail", C.detail_texture);
        C.r_End();
        S.olr_r1_lightmap = _CreateElement(std::move(forward));

        if (multipass)
        {
            ShaderElement additive;
            C.SH = &additive;
            C.RS.Invalidate();
            C.SetParams(S.E[0]->flags.iPriority, false);
            pcstr lightShader = useDetail
                ? "olr_r1_lightmap_dynamic_detail" : "olr_r1_lightmap_dynamic";
            C.r_Pass(shader, lightShader, FALSE, TRUE, FALSE, TRUE,
                D3DBLEND_ONE, D3DBLEND_ONE);
            C.RS.SetRS(D3DRS_ZFUNC, D3DCMP_EQUAL);
            C.r_Sampler("s_base", C.L_textures[0]);
            C.r_Sampler_clf("s_lmap", C.L_textures[1].c_str());
            C.r_Sampler_clf("s_hemi", C.L_textures[2].c_str());
            C.r_Sampler_rtf("s_tonemap", r2_RT_luminance_cur);
            if (useDetail)
                C.r_Sampler("s_detail", C.detail_texture);
            C.r_End();
            S.olr_r1_lightmap_dynamic = _CreateElement(std::move(additive));
        }

        static u32 logged = 0;
        if (S.olr_r1_lightmap && logged++ < 8)
            Msg("* OLR R1 forward: compiled Lua default lightmap material '%s' (detail=%u)",
                C.L_textures[0].c_str(), unsigned(useDetail));
        }
    }
#endif

    ScopeLock scope(&v_shaders_lock);
    // Search equal in shaders array
    for (Shader* v_shader : v_shaders)
        if (S.equal(v_shader))
            return v_shader;

    // Create _new_ entry
    Shader* N = v_shaders.emplace_back(xr_new<Shader>(S));
    N->dwFlags |= xr_resource_flagged::RF_REGISTERED;
    return N;
}

ShaderElement* CBlender_Compile::_lua_Compile(LPCSTR namesp, LPCSTR name)
{
    using namespace luabind;

    ShaderElement E;
    SH = &E;
    RS.Invalidate();

    // Compile
    LPCSTR t_0 = *L_textures[0] ? *L_textures[0] : "null";
    LPCSTR t_1 = (L_textures.size() > 1) ? *L_textures[1] : "null";
    LPCSTR t_d = detail_texture ? detail_texture : "null";
    const object shader = RImplementation.Resources->ScriptEngine.name_space(namesp);
    const functor<void> element = (object)shader[name];
    adopt_compiler ac = adopt_compiler(this);
    element(ac, t_0, t_1, t_d);
    r_End();
    ShaderElement* _r = RImplementation.Resources->_CreateElement(std::move(E));
    return _r;
}
} // namespace xray::render::RENDER_NAMESPACE
