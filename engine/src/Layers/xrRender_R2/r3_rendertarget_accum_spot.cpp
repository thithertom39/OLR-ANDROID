#include "stdafx.h"
#include "Layers/xrRender/du_cone.h"

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
#include "Layers/xrRenderGL/glTextureBindings.h"
#endif

namespace xray::render::RENDER_NAMESPACE
{
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
u32 gles_spot_shadow_audit_ticket(const light& source);

namespace
{
void audit_spot_shadow_matrix_load(u32 ticket, const R_constant& constant, const R_constant_load& load,
    pcstr load_name, GLint current_program, const Fmatrix& shadow)
{
    GLint linked = 0, actual_location = -1, uniform_type = 0, uniform_size = 0;
    GLuint uniform_index = GL_INVALID_INDEX;
    bool mapping_valid = constant.type == RC_float && load.cls == RC_4x4 && load.program &&
        load.location != GLuint(-1) && glIsProgram(load.program);
    if (mapping_valid)
    {
        glGetProgramiv(load.program, GL_LINK_STATUS, &linked);
        mapping_valid = linked == GL_TRUE;
    }
    if (mapping_valid)
    {
        actual_location = glGetUniformLocation(load.program, "m_shadow");
        mapping_valid = actual_location >= 0 && GLuint(actual_location) == load.location;
    }
    if (mapping_valid)
    {
        const GLchar* name = "m_shadow";
        glGetUniformIndices(load.program, 1, &name, &uniform_index);
        mapping_valid = uniform_index != GL_INVALID_INDEX;
        if (mapping_valid)
        {
            glGetActiveUniformsiv(load.program, 1, &uniform_index, GL_UNIFORM_TYPE, &uniform_type);
            glGetActiveUniformsiv(load.program, 1, &uniform_index, GL_UNIFORM_SIZE, &uniform_size);
            mapping_valid = uniform_type == GL_FLOAT_MAT4 && uniform_size == 1;
        }
    }
    Msg("* [gles_spot_shadow_audit] ticket=%u stage=matrix_mapping load=%s destination=0x%x "
        "type=%u class=%u program=%u linked=%u current_program=%d program_active=%u location=%u "
        "actual_location=%d uniform_type=0x%x uniform_size=%d mapping_valid=%u",
        ticket, load_name, constant.destination, u32(constant.type), u32(load.cls), load.program, u32(linked),
        current_program, u32(load.program == GLuint(current_program)), load.location, actual_location,
        u32(uniform_type), uniform_size, u32(mapping_valid));
    if (!mapping_valid)
        return;

    GLfloat gpu[16]{};
    glGetUniformfv(load.program, actual_location, gpu);
    bool finite = true;
    float max_error = 0.f;
    u32 max_error_index = 0;
    for (u32 i = 0; i < 16; ++i)
    {
        // glGetUniformfv returns column-major values. The engine's row-vector
        // Fmatrix storage is the expected sequence for the current upload path.
        const float expected = shadow.m[i / 4][i % 4];
        if (!_valid(gpu[i]) || !_valid(expected))
        {
            finite = false;
            continue;
        }
        const float error = _abs(gpu[i] - expected);
        if (error > max_error)
        {
            max_error = error;
            max_error_index = i;
        }
    }
    Msg("* [gles_spot_shadow_audit] ticket=%u stage=matrix_gpu load=%s finite=%u "
        "max_error=%g max_error_index=%u values=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g",
        ticket, load_name, u32(finite), max_error, max_error_index,
        gpu[0], gpu[1], gpu[2], gpu[3], gpu[4], gpu[5], gpu[6], gpu[7],
        gpu[8], gpu[9], gpu[10], gpu[11], gpu[12], gpu[13], gpu[14], gpu[15]);
}

void audit_spot_shadow_sampling(CBackend& cmd_list, const light& source, const Fmatrix& shadow,
    u32 shader_id, GLuint desired_depth)
{
    static u32 passes_logged = 0;
    if (passes_logged >= 16)
        return;
    const u32 ticket = gles_spot_shadow_audit_ticket(source);
    if (!ticket)
        return;
    ++passes_logged;
    GLint current_program = 0, active_texture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
    Msg("* [gles_spot_shadow_audit] ticket=%u stage=accum frame=%u light=%p context=%u shader_id=%u "
        "program=%d atlas=%u,%u,%u desired_depth=%u matrix_finite=%u depth_scale=%g depth_bias=%g",
        ticket, Device.dwFrame, static_cast<const void*>(&source), cmd_list.context_id, shader_id,
        current_program, source.X.S.posX, source.X.S.posY, source.X.S.size, desired_depth,
        u32(_valid(shadow)), ps_r2_ls_depth_scale, ps_r2_ls_depth_bias);
    Msg("* [gles_spot_shadow_audit] ticket=%u m_shadow=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g",
        ticket, shadow._11, shadow._12, shadow._13, shadow._14, shadow._21, shadow._22, shadow._23, shadow._24,
        shadow._31, shadow._32, shadow._33, shadow._34, shadow._41, shadow._42, shadow._43, shadow._44);

    const auto matrix = cmd_list.get_c("m_shadow");
    if (matrix)
    {
        if (matrix->destination & RC_dest_all)
            audit_spot_shadow_matrix_load(ticket, *matrix, matrix->pp, "pp", current_program, shadow);
        if (matrix->destination & RC_dest_pixel)
            audit_spot_shadow_matrix_load(ticket, *matrix, matrix->ps, "ps", current_program, shadow);
        if (matrix->destination & RC_dest_vertex)
            audit_spot_shadow_matrix_load(ticket, *matrix, matrix->vs, "vs", current_program, shadow);
    }
    else
        Msg("* [gles_spot_shadow_audit] ticket=%u stage=matrix_mapping m_shadow_found=0", ticket);

    const auto smap = cmd_list.get_c("s_smap");
    if (!smap || smap->type != RC_sampler || !smap->samp.program ||
        smap->samp.location == GLuint(-1) || !glIsProgram(smap->samp.program))
    {
        Msg("* [gles_spot_shadow_audit] ticket=%u stage=sampler s_smap_mapping_valid=0", ticket);
        return;
    }
    GLint uniform_unit = -1, max_units = 0;
    glGetUniformiv(smap->samp.program, GLint(smap->samp.location), &uniform_unit);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_units);
    const bool unit_valid = uniform_unit >= 0 && uniform_unit < max_units;
    GLuint mapped_texture = 0, cached_texture = 0, sampler = 0;
    bool cached_texture_valid = false, cached_sampler_valid = false;
    if (unit_valid && u32(uniform_unit) < CTexture::rstGeometry)
    {
        if (const auto texture = cmd_list.get_ActiveTexture(u32(uniform_unit)))
            mapped_texture = texture->surface_get();
    }
    if (unit_valid && u32(uniform_unit) < GLTextureBindings::detail::Capacity)
    {
        // A cache observation is explicitly distinguished from a driver query.
        const auto& bindings = GLTextureBindings::detail::State();
        const auto& texture = bindings.textures[uniform_unit][GLTextureBindings::detail::TargetIndex(GL_TEXTURE_2D)];
        cached_texture = texture.name;
        cached_texture_valid = texture.valid;
        sampler = bindings.samplers[uniform_unit].name;
        cached_sampler_valid = bindings.samplers[uniform_unit].valid;
        if (!cached_sampler_valid)
            sampler = 0;
    }
    GLint actual_texture = -1;
    if (unit_valid)
    {
        GLint actual_sampler = 0;
        // Temporarily select the real uniform unit for driver queries. Do not
        // change any binding or use the cache helper; restore the exact active
        // unit below so the shared active-unit cache remains correct.
        if (active_texture != GL_TEXTURE0 + uniform_unit)
            glActiveTexture(GL_TEXTURE0 + uniform_unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &actual_texture);
        glGetIntegerv(GL_SAMPLER_BINDING, &actual_sampler);
        sampler = GLuint(actual_sampler);
    }
    GLint compare_mode = -1, compare_func = -1, min_filter = -1, mag_filter = -1;
    bool compare_known = false;
    if (sampler && glIsSampler(sampler))
    {
        glGetSamplerParameteriv(sampler, GL_TEXTURE_COMPARE_MODE, &compare_mode);
        glGetSamplerParameteriv(sampler, GL_TEXTURE_COMPARE_FUNC, &compare_func);
        glGetSamplerParameteriv(sampler, GL_TEXTURE_MIN_FILTER, &min_filter);
        glGetSamplerParameteriv(sampler, GL_TEXTURE_MAG_FILTER, &mag_filter);
        compare_known = true;
    }
    else if (unit_valid && sampler == 0 && actual_texture > 0)
    {
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, &compare_mode);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, &compare_func);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_filter);
        compare_known = true;
    }
    if (unit_valid && active_texture != GL_TEXTURE0 + uniform_unit)
        glActiveTexture(GLenum(active_texture));
    Msg("* [gles_spot_shadow_audit] ticket=%u stage=sampler sampler_program=%u location=%u "
        "mapped_unit=%u actual_uniform_unit=%d unit_valid=%u active_unit=%d binding_query_actual=%u "
        "mapped_texture=%u cached_texture=%u cache_texture_valid=%u actual_texture=%d "
        "sampler=%u sampler_source=%s cache_sampler_valid=%u compare_known=%u "
        "compare_mode=0x%x compare_func=0x%x min_filter=0x%x mag_filter=0x%x",
        ticket, smap->samp.program, smap->samp.location, u32(smap->samp.index), uniform_unit, u32(unit_valid),
        active_texture - GL_TEXTURE0, u32(unit_valid), mapped_texture, cached_texture, u32(cached_texture_valid),
        actual_texture, sampler, unit_valid ? "driver" : "cache", u32(cached_sampler_valid), u32(compare_known),
        u32(compare_mode), u32(compare_func), u32(min_filter), u32(mag_filter));
}
} // namespace
#endif

// extern Fvector du_cone_vertices[DU_CONE_NUMVERTEX];

void CRenderTarget::accum_spot(CBackend& cmd_list, light* L, bool allow_shadow)
{
    phase_accumulator(cmd_list);
    RImplementation.Stats.l_visible++;

    // *** assume accumulator already setup ***
    // *****************************	Mask by stencil		*************************************
    ref_shader shader;
    ref_shader* shader_msaa;
    if (IRender_Light::OMNIPART == L->flags.type)
    {
        shader = L->s_point;
        shader_msaa = L->s_point_msaa;
        if (!shader)
        {
            shader = s_accum_point;
            shader_msaa = s_accum_point_msaa;
        }
    }
    else
    {
        shader = L->s_spot;
        shader_msaa = L->s_spot_msaa;
        if (!shader)
        {
            shader = s_accum_spot;
            shader_msaa = s_accum_spot_msaa;
        }
    }

    {
        // setup xform
        L->xform_calc();
        cmd_list.set_xform_world(L->m_xform);
        cmd_list.set_xform_view(Device.mView);
        cmd_list.set_xform_project(Device.mProject);
        enable_scissor(L);
        enable_dbt_bounds(L);

        // *** similar to "Carmack's reverse", but assumes convex, non intersecting objects,
        // *** thus can cope without stencil clear with 127 lights
        // *** in practice, 'cause we "clear" it back to 0x1 it usually allows us to > 200 lights :)
        //	Done in blender!
        // cmd_list.set_ColorWriteEnable		(FALSE);
        cmd_list.set_Element(s_accum_mask->E[SE_MASK_SPOT]); // masker

        // backfaces: if (stencil>=1 && zfail)			stencil = light_id
        cmd_list.set_CullMode(CULL_CW);
        if (!RImplementation.o.msaa)
        {
            cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, dwLightMarkerID, 0x01, 0xff,
                D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE);
        }
        else
        {
            cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, dwLightMarkerID, 0x01, 0x7f,
                D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE);
        }
        draw_volume(cmd_list, L);

        // frontfaces: if (stencil>=light_id && zfail)	stencil = 0x1
        cmd_list.set_CullMode(CULL_CCW);
        if (!RImplementation.o.msaa)
        {
            cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0xff, 0xff,
                D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE);
        }
        else
        {
            cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0x7f, 0x7f,
                D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE);
        }
        draw_volume(cmd_list, L);
    }

    // nv-stencil recompression
    if (RImplementation.o.nvstencil)
        u_stencil_optimize(cmd_list);

    // *****************************	Minimize overdraw	*************************************
    // Select shader (front or back-faces), *** back, if intersect near plane
    cmd_list.set_ColorWriteEnable();
    cmd_list.set_CullMode(CULL_CW); // back

    // 2D texgens
    Fmatrix m_Texgen;
    u_compute_texgen_screen(cmd_list, m_Texgen);
    Fmatrix m_Texgen_J;
    u_compute_texgen_jitter(cmd_list, m_Texgen_J);

    // Shadow xform (+texture adjustment matrix)
    Fmatrix m_Shadow, m_Lmap;
    {
        float smapsize = float(RImplementation.o.smapsize);
        float fTexelOffs = (.5f / smapsize);
        float view_dim = float(L->X.S.size - 2) / smapsize;
        float view_sx = float(L->X.S.posX + 1) / smapsize;
        float view_sy = float(L->X.S.posY + 1) / smapsize;
        float fRange = float(1.f) * ps_r2_ls_depth_scale;
        float fBias = ps_r2_ls_depth_bias;
#ifdef USE_DX11
        Fmatrix m_TexelAdjust =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, -view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, fBias, 1.0f
        };
#elif defined(USE_OGL)
        Fmatrix m_TexelAdjust =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f * fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, 0.5f + fBias, 1.0f
        };
#else
#   error No graphics API selected or enabled!
#endif
        // compute xforms
        Fmatrix xf_view = L->X.S.view;
        Fmatrix xf_project;
        xf_project.mul(m_TexelAdjust, L->X.S.project);
        m_Shadow.mul(xf_view, Device.mInvView);
        m_Shadow.mulA_44(xf_project);

        // lmap
        view_dim = 1.f;
        view_sx = 0.f;
        view_sy = 0.f;
#ifdef USE_DX11
        Fmatrix m_TexelAdjust2 =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, -view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, fBias, 1.0f
        };
#elif defined(USE_OGL)
        Fmatrix m_TexelAdjust2 =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f * fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, 0.5f + fBias, 1.0f
        };
#else
#   error No graphics API selected or enabled!
#endif

        // compute xforms
        xf_project.mul(m_TexelAdjust2, L->X.S.project);
        m_Lmap.mul(xf_view, Device.mInvView);
        m_Lmap.mulA_44(xf_project);
    }

    // Common constants
    Fvector L_dir, L_clr, L_pos;
    float L_spec;
    L_clr.set(L->color.r, L->color.g, L->color.b);
    L_clr.mul(L->get_LOD());
    L_spec = u_diffuse2s(L_clr);
    Device.mView.transform_tiny(L_pos, L->position);
    Device.mView.transform_dir(L_dir, L->direction);
    L_dir.normalize();

    // Draw volume with projective texgen
    {
        // Select shader
        u32 _id = 0;
        if (allow_shadow && L->flags.bShadow)
        {
            bool bFullSize = (L->X.S.size == RImplementation.o.smapsize);
            if (L->X.S.transluent)
                _id = SE_L_TRANSLUENT;
            else if (bFullSize)
                _id = SE_L_FULLSIZE;
            else
                _id = SE_L_NORMAL;
        }
        else
        {
            _id = SE_L_UNSHADOWED;
            m_Shadow = m_Lmap;
        }
        cmd_list.set_Element(shader->E[_id]);

        cmd_list.set_CullMode(CULL_CW); // back

        // Constants
        float att_R = L->range * .95f;
        float att_factor = 1.f / (att_R * att_R);
        cmd_list.set_c("Ldynamic_pos", L_pos.x, L_pos.y, L_pos.z, att_factor);
        cmd_list.set_c("Ldynamic_color", L_clr.x, L_clr.y, L_clr.z, L_spec);
        cmd_list.set_c("m_texgen", m_Texgen);
        cmd_list.set_c("m_texgen_J", m_Texgen_J);
        cmd_list.set_c("m_shadow", m_Shadow);
        cmd_list.set_ca("m_lmap", 0, m_Lmap._11, m_Lmap._21, m_Lmap._31, m_Lmap._41);
        cmd_list.set_ca("m_lmap", 1, m_Lmap._12, m_Lmap._22, m_Lmap._32, m_Lmap._42);

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
        audit_spot_shadow_sampling(cmd_list, *L, m_Shadow, _id, rt_smap_depth->pZRT);
#endif

        // Fetch4 : enable
        //		if (RImplementation.o.HW_smap_FETCH4)	{
        //. we hacked the shader to force smap on S0
        //#			define FOURCC_GET4  MAKEFOURCC('G','E','T','4')
        //			HW.pDevice->SetSamplerState	( 0, D3DSAMP_MIPMAPLODBIAS, FOURCC_GET4 );
        //		}

        if (!RImplementation.o.msaa)
        {
            cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, dwLightMarkerID, 0xff, 0x00);
            draw_volume(cmd_list, L);
        }
        else
        {
            // per pixel
            cmd_list.set_Element(shader->E[_id]);
            cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID, 0xff, 0x00);
            cmd_list.set_CullMode(D3DCULL_CW);
            draw_volume(cmd_list, L);

            // per sample
            if (RImplementation.o.msaa_opt)
            {
                cmd_list.set_Element(shader_msaa[0]->E[_id]);
                cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID | 0x80, 0xff, 0x00);
                cmd_list.set_CullMode(D3DCULL_CW);
                draw_volume(cmd_list, L);
            }
            else // checked Holger
            {
#ifdef USE_DX11
                for (u32 i = 0; i < RImplementation.o.msaa_samples; ++i)
                {
                    cmd_list.set_Element(shader_msaa[i]->E[_id]);
                    cmd_list.StateManager.SetSampleMask(u32(1) << i);
                    cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID | 0x80, 0xff, 0x00);
                    cmd_list.set_CullMode(D3DCULL_CW);
                    draw_volume(cmd_list, L);
                }
                cmd_list.StateManager.SetSampleMask(0xffffffff);
#elif defined(USE_OGL)
                VERIFY(!"Only optimized MSAA is supported in OpenGL");
#else
#   error No graphics API selected or enabled!
#endif
            }
            cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, dwLightMarkerID, 0xff, 0x00);
        }

        // Fetch4 : disable
        //		if (RImplementation.o.HW_smap_FETCH4)	{
        //. we hacked the shader to force smap on S0
        //#			define FOURCC_GET1  MAKEFOURCC('G','E','T','1')
        //			HW.pDevice->SetSamplerState	( 0, D3DSAMP_MIPMAPLODBIAS, FOURCC_GET1 );
        //		}
    }

    // blend-copy
    if (!RImplementation.o.fp16_blend)
    {
        u_setrt(cmd_list, rt_Accumulator, nullptr, nullptr, rt_MSAADepth);
        cmd_list.set_Element(s_accum_mask->E[SE_MASK_ACCUM_VOL]);
        cmd_list.set_c("m_texgen", m_Texgen);
        cmd_list.set_c("m_texgen_J", m_Texgen_J);
        if (!RImplementation.o.msaa)
        {
            cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID, 0xff, 0x00);
            draw_volume(cmd_list, L);
        }
        else // checked Holger
        {
            // per pixel
            cmd_list.set_Element(s_accum_mask->E[SE_MASK_ACCUM_VOL]);
            cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID, 0xff, 0x00);
            draw_volume(cmd_list, L);
            // per sample
            if (RImplementation.o.msaa_opt)
            {
                cmd_list.set_Element(s_accum_mask_msaa[0]->E[SE_MASK_ACCUM_VOL]);
                cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID | 0x80, 0xff, 0x00);
                draw_volume(cmd_list, L);
            }
            else // checked Holger
            {
#ifdef USE_DX11
                for (u32 i = 0; i < RImplementation.o.msaa_samples; ++i)
                {
                    cmd_list.set_Element(s_accum_mask_msaa[i]->E[SE_MASK_ACCUM_VOL]);
                    cmd_list.StateManager.SetSampleMask(u32(1) << i);
                    cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID | 0x80, 0xff, 0x00);
                    draw_volume(cmd_list, L);
                }
                cmd_list.StateManager.SetSampleMask(0xffffffff);
#elif defined(USE_OGL)
                VERIFY(!"Only optimized MSAA is supported in OpenGL");
#else
#   error No graphics API selected or enabled!
#endif
            }
            cmd_list.set_Stencil(TRUE, D3DCMP_EQUAL, dwLightMarkerID, 0xff, 0x00);
        }
    }

    cmd_list.set_Scissor(0);
    // dwLightMarkerID					+=	2;	// keep lowest bit always setted up
    increment_light_marker(cmd_list);

    u_DBT_disable();
}

void CRenderTarget::accum_volumetric(CBackend& cmd_list, light* L)
{
    // if (L->flags.type != IRender_Light::SPOT) return;
    if (!L->flags.bVolumetric)
        return;

    phase_vol_accumulator(cmd_list);

    ref_shader shader;
    //ref_shader* shader_msaa;

    shader = L->s_volumetric;
    //shader_msaa = L->s_volumetric_msaa;
    if (!shader)
    {
        shader = s_accum_volume;
        //shader_msaa = s_accum_volume_msaa;
    }

    // *** assume accumulator setted up ***
    // *****************************	Mask by stencil		*************************************
    {
        // setup xform
        L->xform_calc();
        cmd_list.set_xform_world(L->m_xform);
        cmd_list.set_xform_view(Device.mView);
        cmd_list.set_xform_project(Device.mProject);
        enable_scissor(L);

        // enable_dbt_bounds				(L);
    }

    cmd_list.set_ColorWriteEnable();
    cmd_list.set_CullMode(CULL_NONE); // back

    // 2D texgens
    Fmatrix m_Texgen;
    u_compute_texgen_screen(cmd_list, m_Texgen);
    Fmatrix m_Texgen_J;
    u_compute_texgen_jitter(cmd_list, m_Texgen_J);

    // Shadow xform (+texture adjustment matrix)
    Fmatrix m_Shadow, m_Lmap;
    Fmatrix mFrustumSrc;
    CFrustum ClipFrustum;
    {
        float smapsize = float(RImplementation.o.smapsize);
        float fTexelOffs = (.5f / smapsize);
        float view_dim = float(L->X.S.size - 2) / smapsize;
        float view_sx = float(L->X.S.posX + 1) / smapsize;
        float view_sy = float(L->X.S.posY + 1) / smapsize;
        float fRange = float(1.f) * ps_r2_ls_depth_scale;
        float fBias = ps_r2_ls_depth_bias;
#ifdef USE_DX11
        Fmatrix m_TexelAdjust =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, -view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, fBias, 1.0f
        };
#elif defined(USE_OGL)
        Fmatrix m_TexelAdjust =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f * fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, 0.5f + fBias, 1.0f
        };
#else
#   error No graphics API selected or enabled!
#endif

        // compute xforms
        Fmatrix xf_view = L->X.S.view;
        Fmatrix xf_project;
        xf_project.mul(m_TexelAdjust, L->X.S.project);
        m_Shadow.mul(xf_view, Device.mInvView);
        m_Shadow.mulA_44(xf_project);

        // lmap
        view_dim = 1.f;
        view_sx = 0.f;
        view_sy = 0.f;
#ifdef USE_DX11
        Fmatrix m_TexelAdjust2 =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, -view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, fBias, 1.0f
        };
#elif defined(USE_OGL)
        Fmatrix m_TexelAdjust2 =
        {
            view_dim / 2.f, 0.0f, 0.0f, 0.0f,
            0.0f, view_dim / 2.f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f * fRange, 0.0f,
            view_dim / 2.f + view_sx + fTexelOffs, view_dim / 2.f + view_sy + fTexelOffs, 0.5f + fBias, 1.0f
        };
#else
#   error No graphics API selected or enabled!
#endif

        // compute xforms
        xf_project.mul(m_TexelAdjust2, L->X.S.project);
        m_Lmap.mul(xf_view, Device.mInvView);
        m_Lmap.mulA_44(xf_project);

        // Compute light frustum in world space
        mFrustumSrc.mul(L->X.S.project, xf_view);
        ClipFrustum.CreateFromMatrix(mFrustumSrc, FRUSTUM_P_ALL);
        //	Adjust frustum far plane
        //	4 - far, 5 - near
        ClipFrustum.planes[4].d -= (ClipFrustum.planes[4].d + ClipFrustum.planes[5].d) * (1 - L->m_volumetric_distance);
    }

    //	Calculate camera space AABB
    // xform BB
    /*
    Fbox	BB;
    Fvector	rr; rr.set(L->spatial.sphere.R,L->spatial.sphere.R,L->spatial.sphere.R);
    BB.setb	(L->spatial.sphere.P, rr);

    Fbox	bbp; bbp.invalidate();
    for (u32 i=0; i<8; i++)		{
        Fvector		pt;
        BB.getpoint	(i,pt);
        //Device.mFullTransform.transform	(pt);
        Device.mFullTransform.transform	(mView);
        bbp.modify	(pt);
    }
    */

    //	Calculate camera space AABB
    //	Adjust AABB according to the adjusted distance for the light volume
    Fbox aabb;

    // float	scaledRadius = L->spatial.sphere.R * (1+L->m_volumetric_distance)*0.5f;
    float scaledRadius = L->spatial.sphere.R * L->m_volumetric_distance;
    Fvector rr = Fvector().set(scaledRadius, scaledRadius, scaledRadius);
    Fvector pt = L->spatial.sphere.P;
    pt.sub(L->position);
    pt.mul(L->m_volumetric_distance);
    pt.add(L->position);
    //	Don't adjust AABB
    // float	scaledRadius = L->spatial.sphere.R;
    // Fvector	rr = Fvector().set(scaledRadius,scaledRadius,scaledRadius);
    // Fvector pt = L->spatial.sphere.P;
    Device.mView.transform(pt);
    aabb.setb(pt, rr);
    /*
        //	Calculate presise AABB assuming we are drawing for the spot light
        {
            aabb.invalidate();
            Fmatrix	transform;
            transform.mul( Device.mView, L->m_xform);
            for (u32 i=0; i<DU_CONE_NUMVERTEX; ++i)
            {
                Fvector		pt = du_cone_vertices[i];
                transform.transform(pt);
                aabb.modify(pt);
            }
        }
    */
    // Common constants
    float fQuality = L->m_volumetric_quality;
    int iNumSlises = int(float(VOLUMETRIC_SLICES) * fQuality);
    //			min 10 surfaces
    iNumSlises = _max(10, iNumSlises);
    //	Adjust slice intensity
    fQuality = float(iNumSlises) / float(VOLUMETRIC_SLICES);
    Fvector L_dir, L_clr, L_pos;
    float L_spec;
    L_clr.set(L->color.r, L->color.g, L->color.b);
    L_clr.mul(L->m_volumetric_intensity);
    L_clr.mul(L->m_volumetric_distance);
    L_clr.mul(1 / fQuality);
    L_clr.mul(L->get_LOD());
    L_spec = u_diffuse2s(L_clr);
    Device.mView.transform_tiny(L_pos, L->position);
    Device.mView.transform_dir(L_dir, L->direction);
    L_dir.normalize();

    // Draw volume with projective texgen
    {
        /*
        // Select shader
        u32		_id					= 0;
        if (L->flags.bShadow)		{
            bool	bFullSize			= (L->X.S.size == RImplementation.o.smapsize);
            if (L->X.S.transluent)	_id	= SE_L_TRANSLUENT;
            else if		(bFullSize)	_id	= SE_L_FULLSIZE;
            else					_id	= SE_L_NORMAL;
        } else {
            _id						= SE_L_UNSHADOWED;
            m_Shadow				= m_Lmap;
        }
        cmd_list.set_Element			(shader->E[ _id ]	);
        */

        cmd_list.set_Element(shader->E[0]);

        // Constants
        float att_R = L->m_volumetric_distance * L->range * .95f;
        float att_factor = 1.f / (att_R * att_R);
        cmd_list.set_c("Ldynamic_pos", L_pos.x, L_pos.y, L_pos.z, att_factor);
        cmd_list.set_c("Ldynamic_color", L_clr.x, L_clr.y, L_clr.z, L_spec);
        cmd_list.set_c("m_texgen", m_Texgen);
        cmd_list.set_c("m_texgen_J", m_Texgen_J);
        cmd_list.set_c("m_shadow", m_Shadow);
        cmd_list.set_ca("m_lmap", 0, m_Lmap._11, m_Lmap._21, m_Lmap._31, m_Lmap._41);
        cmd_list.set_ca("m_lmap", 1, m_Lmap._12, m_Lmap._22, m_Lmap._32, m_Lmap._42);
        cmd_list.set_c("vMinBounds", aabb.x1, aabb.y1, aabb.z1, 0.f);
        //	Increase camera-space aabb z size to compensate decrease of slices number
        cmd_list.set_c("vMaxBounds", aabb.x2, aabb.y2, aabb.z1 + (aabb.z2 - aabb.z1) / fQuality, 0.f);

        //	Set up user clip planes
        {
            static shared_str strFrustumClipPlane("FrustumClipPlane");
            //	TODO: DX11: Check if it's equivalent to the previouse code.
            // cmd_list.set_ClipPlanes (TRUE,ClipFrustum.planes,ClipFrustum.p_count);

            //	Transform frustum to clip space
            Fmatrix PlaneTransform;
            PlaneTransform.transpose(Device.mInvFullTransform);
            // HW.pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0x3F);

            for (int i = 0; i < 6; ++i)
            {
                Fvector4& ClipPlane = *(Fvector4*)&ClipFrustum.planes[i].n.x;
                Fvector4 TransformedPlane;
                PlaneTransform.transform(TransformedPlane, ClipPlane);
                TransformedPlane.mul(-1.0f);
                cmd_list.set_ca(strFrustumClipPlane, i, TransformedPlane);
                // HW.pDevice->SetClipPlane( i, &TransformedPlane.x);
            }
            /*
            for ( int i=0; i<6; ++i)
            {
                Fvector4	TransformedPlane;
                ClipTransform.transform(TransformedPlane, UnitClipPlanes[i]);
                TransformedPlane.normalize_as_plane();
                cmd_list.set_ca(strOOBBClipPlane, i, TransformedPlane);
            }
            */

            /*
            //	Transform frustum to clip space
            Fmatrix PlaneTransform;
            PlaneTransform.transpose(Device.mInvFullTransform);
            HW.pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0x3F);

            for ( int i=0; i<6; ++i)
            {
                Fvector4	&ClipPlane = *(Fvector4*)&ClipFrustum.planes[i].n.x;
                Fvector4	TransformedPlane;
                PlaneTransform.transform(TransformedPlane, ClipPlane);
                TransformedPlane.mul(-1.0f);
                HW.pDevice->SetClipPlane( i, &TransformedPlane.x);
            }
            */
        }

        /*
        float	clip[4];
        clip[0] = 1;
        clip[1] =
        clip[2] =
        clip[3] = 0;
        HW.pDevice->SetClipPlane( 0, clip);
        */

        // Fetch4 : enable
        //		if (RImplementation.o.HW_smap_FETCH4)	{
        //. we hacked the shader to force smap on S0
        //#			define FOURCC_GET4  MAKEFOURCC('G','E','T','4')
        //			HW.pDevice->SetSamplerState	( 0, D3DSAMP_MIPMAPLODBIAS, FOURCC_GET4 );
        //		}

        cmd_list.set_ColorWriteEnable(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

        cmd_list.set_Geometry(g_accum_volumetric);
        //	Igor: no need to do it per sub-sample. Plain AA will go just fine.
        cmd_list.Render(D3DPT_TRIANGLELIST, 0, 0, VOLUMETRIC_SLICES * 4, 0, VOLUMETRIC_SLICES * 2);

        /*
        if( !RImplementation.o.msaa )
            cmd_list.Render(D3DPT_TRIANGLELIST,0,0,iNumSlises*4,0,iNumSlises*2);
        else
        {
            // per pixel
            cmd_list.set_Element(shader->E[0]);
            cmd_list.set_Stencil(TRUE,D3DCMP_EQUAL,dwLightMarkerID,0xff,0x00);
            cmd_list.Render(D3DPT_TRIANGLELIST,0,0,iNumSlises*4,0,iNumSlises*2);

            // per sample
            if( RImplementation.o.msaa_opt )
            {
                // per sample
                cmd_list.set_Element	(shader_msaa[0]->E[0]);
                cmd_list.set_Stencil(TRUE,D3DCMP_EQUAL,dwLightMarkerID|0x80,0xff,0x00);
                cmd_list.Render(D3DPT_TRIANGLELIST,0,0,iNumSlises*4,0,iNumSlises*2);
            }
            else
            {
#ifdef USE_DX11
                for( u32 i = 0; i < RImplementation.o.msaa_samples; ++i )
                {
                    cmd_list.set_Element	      (shader_msaa[i]->E[0]);
                    StateManager.SetSampleMask ( u32(1) << i );
                    cmd_list.set_Stencil         (TRUE,D3DCMP_EQUAL,dwLightMarkerID|0x80,0xff,0x00);
                    cmd_list.Render(D3DPT_TRIANGLELIST,0,0,iNumSlises*4,0,iNumSlises*2);
                }
                StateManager.SetSampleMask( 0xffffffff );
#elif defined(USE_OGL)
                VERIFY(!"Only optimized MSAA is supported in OpenGL");
#else
#   error No graphics API selected or enabled!
#endif
            }
        }*/

        cmd_list.set_ColorWriteEnable();

        // Fetch4 : disable
        //		if (RImplementation.o.HW_smap_FETCH4)	{
        //. we hacked the shader to force smap on S0
        //#			define FOURCC_GET1  MAKEFOURCC('G','E','T','1')
        //			HW.pDevice->SetSamplerState	( 0, D3DSAMP_MIPMAPLODBIAS, FOURCC_GET1 );
        //		}

        //	Restore clip planes
        cmd_list.set_ClipPlanes(FALSE, (Fmatrix*)0, 0);
    }
    /*
        // blend-copy
        if (!RImplementation.o.fp16_blend)	{
            u_setrt						(rt_Accumulator,NULL,NULL,get_base_zb());
            cmd_list.set_Element			(s_accum_mask->E[SE_MASK_ACCUM_VOL]	);
            cmd_list.set_c				("m_texgen",		m_Texgen);
            cmd_list.set_c				("m_texgen_J",		m_Texgen_J	);
            draw_volume					(L);
        }
    */
    cmd_list.set_Scissor(0);
}
} // namespace xray::render::RENDER_NAMESPACE
