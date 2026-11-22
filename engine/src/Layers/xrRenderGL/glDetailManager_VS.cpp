#include "stdafx.h"
#include "Layers/xrRender/DetailManager.h"
#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/Environment.h"
#include "Layers/xrRender/BufferUtils.h"
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
#include "glDetailUniformBatch.h"
#include "glDetailShadowCull.h"
#if defined(XR_PLATFORM_ANDROID)
#include "Layers/xrRender/olr_r1_mode.h"
#endif
#include <cstring>
#endif
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include "glGpuProfiler.h"
#endif

namespace xray::render::RENDER_NAMESPACE
{
namespace detail_manager
{
extern const int quant;
//extern const int c_hdr;
}

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))

namespace
{
thread_local bool nativeR1DetailPass = false;
thread_local const GLDetailShadowCull::PlaneSet* detailLocalShadowPlanes = nullptr;
thread_local const CBackend* detailLocalShadowBackend = nullptr;
bool DetailLocalShadowCullRequested()
{
    // Core.Params can change when the engine is initialized again in the
    // same Android process. Do not cache the first session's flag value.
    if (!Core.Params) return false;
    constexpr char flag[] = "-gles_no_detail_light_cull";
    for (const char* p = Core.Params; (p = std::strstr(p, flag)) != nullptr; ++p)
        if ((p == Core.Params || std::strchr(" \t\r\n", p[-1])) &&
            (p[sizeof(flag)-1] == '\0' || std::strchr(" \t\r\n", p[sizeof(flag)-1])))
            return false;
    // On by default. The former positive flag remains compatible; an exact
    // negative flag always takes precedence, regardless of argument order.
    return true;
}
struct DetailLocalShadowScope
{
    const GLDetailShadowCull::PlaneSet* previous;
    const CBackend* previousBackend;
    DetailLocalShadowScope(const GLDetailShadowCull::PlaneSet* value, const CBackend* backend):
        previous(detailLocalShadowPlanes), previousBackend(detailLocalShadowBackend)
    {
        detailLocalShadowPlanes = value;
        detailLocalShadowBackend = value ? backend : nullptr;
    }
    ~DetailLocalShadowScope()
    {
        detailLocalShadowPlanes = previous;
        detailLocalShadowBackend = previousBackend;
    }
};
}

#if defined(XR_PLATFORM_ANDROID)
void CDetailManager::RenderNativeR1(CBackend& cmd_list)
{
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope detailsScope(GLGpuProfiler::Stage::R1Details);
#endif
    struct Scope
    {
        bool previous = nativeR1DetailPass;
        Scope() { nativeR1DetailPass = true; }
        ~Scope() { nativeR1DetailPass = previous; }
    } scope;
    Render(cmd_list);
}
#endif

void CDetailManager::RenderShadow(CBackend& cmd_list, const CFrustum& shadowFrustum)
{
    if (!DetailLocalShadowCullRequested())
    {
        Render(cmd_list);
        return;
    }
#ifndef _EDITOR
    if (!dtFS || !psDeviceFlags.is(rsDrawDetails))
    {
        Render(cmd_list);
        return;
    }
#endif
    // Wait may help other tasks. Do that before installing this local-light
    // scope; Render's original wait will then see the completed calculation.
    TaskScheduler->Wait(*m_calc_task);
    const GLDetailShadowCull::PlaneSet planes(shadowFrustum.planes, shadowFrustum.p_count);
    DetailLocalShadowScope scope(planes.Valid() ? &planes : nullptr, &cmd_list);
    Render(cmd_list);
}
#endif

void CDetailManager::hw_Load_Shaders()
{
    // Create shader to access constant storage
    ref_shader S;
    S.create("details\\set");
    if (!S || !S->E[0] || S->E[0]->passes.empty())
        S.create("details" DELIMITER "set");

    if (S && S->E[0] && !S->E[0]->passes.empty() && S->E[0]->passes[0]->constants)
    {
        R_constant_table& T0 = *S->E[0]->passes[0]->constants;
        hwc_consts = T0.get("consts");
        hwc_wave = T0.get("wave");
        hwc_wind = T0.get("dir2D");
        hwc_array = T0.get("array");
        if (!hwc_array) hwc_array = T0.get("array[0]");
    }
    if (S && S->E[1] && !S->E[1]->passes.empty() && S->E[1]->passes[0]->constants)
    {
        R_constant_table& T1 = *S->E[1]->passes[0]->constants;
        hwc_s_consts = T1.get("consts");
        hwc_s_xform = T1.get("xform");
        hwc_s_array = T1.get("array");
        if (!hwc_s_array) hwc_s_array = T1.get("array[0]");
    }
}

void CDetailManager::hw_Render(CBackend& cmd_list)
{
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    GLGpuProfiler::Scope detailScope(GLGpuProfiler::Stage::Details, false);
#endif
    using namespace detail_manager;

    // Render-prepare
    //	Update timer
    //	Can't use Device.fTimeDelta since it is smoothed! Don't know why, but smoothed value looks more choppy!
    float fDelta = Device.fTimeGlobal - m_global_time_old;
    if (fDelta < 0 || fDelta > 1)
        fDelta = 0.03f;
    m_global_time_old = Device.fTimeGlobal;

    m_time_rot_1 += PI_MUL_2 * fDelta / swing_current.rot1;
    m_time_rot_2 += PI_MUL_2 * fDelta / swing_current.rot2;
    m_time_pos += fDelta * swing_current.speed;

    //float		tm_rot1		= (PI_MUL_2*Device.fTimeGlobal/swing_current.rot1);
    //float		tm_rot2		= (PI_MUL_2*Device.fTimeGlobal/swing_current.rot2);
    float tm_rot1 = m_time_rot_1;
    float tm_rot2 = m_time_rot_2;

    Fvector4 dir1, dir2;
    dir1.set(_sin(tm_rot1), 0, _cos(tm_rot1), 0).normalize().mul(swing_current.amp1);
    dir2.set(_sin(tm_rot2), 0, _cos(tm_rot2), 0).normalize().mul(swing_current.amp2);

    // Setup geometry and DMA
    cmd_list.set_Geometry(hw_Geom);

    // Wave0
    float scale = 1.f / float(quant);
    Fvector4 wave;
    Fvector4 consts;
    consts.set(scale, scale, ps_r__Detail_l_aniso, ps_r__Detail_l_ambient);
    //wave.set				(1.f/5.f,		1.f/7.f,	1.f/3.f,	Device.fTimeGlobal*swing_current.speed);
    wave.set(1.f / 5.f, 1.f / 7.f, 1.f / 3.f, m_time_pos);
    //RCache.set_c			(&*hwc_consts,	scale,		scale,		ps_r__Detail_l_aniso,	ps_r__Detail_l_ambient);				// consts
    //RCache.set_c			(&*hwc_wave,	wave.div(PI_MUL_2));	// wave
    //RCache.set_c			(&*hwc_wind,	dir1);																					// wind-dir
    //hw_Render_dump			(&*hwc_array,	1, 0, c_hdr );
    hw_Render_dump(cmd_list, consts, wave.div(PI_MUL_2), dir1, 1, 0);

    // Wave1
    //wave.set				(1.f/3.f,		1.f/7.f,	1.f/5.f,	Device.fTimeGlobal*swing_current.speed);
    wave.set(1.f / 3.f, 1.f / 7.f, 1.f / 5.f, m_time_pos);
    //RCache.set_c			(&*hwc_wave,	wave.div(PI_MUL_2));	// wave
    //RCache.set_c			(&*hwc_wind,	dir2);																					// wind-dir
    //hw_Render_dump			(&*hwc_array,	2, 0, c_hdr );
    hw_Render_dump(cmd_list, consts, wave.div(PI_MUL_2), dir2, 2, 0);

    // Still
    consts.set(scale, scale, scale, 1.f);
    //RCache.set_c			(&*hwc_s_consts,scale,		scale,		scale,				1.f);
    //RCache.set_c			(&*hwc_s_xform,	Device.mFullTransform);
    //hw_Render_dump			(&*hwc_s_array,	0, 1, c_hdr );
    hw_Render_dump(cmd_list, consts, wave.div(PI_MUL_2), dir2, 0, 1);
}

void CDetailManager::hw_Render_dump(CBackend& cmd_list, const Fvector4& consts, const Fvector4& wave, const Fvector4& wind, u32 var_id,
                                    u32 lod_id)
{
    static shared_str strConsts("consts");
    static shared_str strWave("wave");
    static shared_str strDir2D("dir2D");
    static shared_str strArray("array");
    static shared_str strXForm("xform");

    RImplementation.BasicStats.DetailCount = 0;

    // Matrices and offsets
    u32 vOffset = 0;
    u32 iOffset = 0;

    vis_list& list = m_visibles [var_id];

    const auto& desc = g_pGamePersistent->Environment().CurrentEnv;
    Fvector c_sun, c_ambient, c_hemi;
    c_sun.set(desc.sun_color.x, desc.sun_color.y, desc.sun_color.z);
    c_sun.mul(.5f);
    c_ambient.set(desc.ambient.x, desc.ambient.y, desc.ambient.z);
    c_hemi.set(desc.hemi_color.x, desc.hemi_color.y, desc.hemi_color.z);

#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)) && defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
    auto* detailCullStats = detailLocalShadowPlanes ? GLGpuProfiler::SampleLocalDetailCull() : nullptr;
#endif

    // Iterate
    for (u32 O = 0; O < objects.size(); O++)
    {
        CDetail& Object = *objects [O];
#if defined(XR_PLATFORM_ANDROID)
        const bool ported = Object.shader->olr_r1_detail_wave && Object.shader->olr_r1_detail_still;
        if (nativeR1DetailPass && !ported) continue;
        const bool mainPhase = RImplementation.get_context(cmd_list.context_id).o.phase == CRender::PHASE_NORMAL;
        if (!nativeR1DetailPass && ported && olr_r1_scene_enabled() && mainPhase) continue;
        auto& element = nativeR1DetailPass ? (lod_id == 0 ?
            Object.shader->olr_r1_detail_wave : Object.shader->olr_r1_detail_still) : Object.shader->E[lod_id];
#else
        auto& element = Object.shader->E[lod_id];
#endif
        xr_vector<SlotItemVec*>& vis = list [O];
        if (!vis.empty())
        {
            for (u32 iPass = 0; iPass < element->passes.size(); ++iPass)
            {
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
                const auto& detailPass = element->passes[iPass];
                const bool cullThisPass = detailLocalShadowPlanes && detailLocalShadowBackend == &cmd_list &&
                    detailPass->pp && GLDetailShadowCull::KnownProgram(detailPass->pp->cName.c_str(), var_id != 0);
                const GLDetailShadowCull::Bounds* cellBounds = nullptr;
                // No shader/material/uniform work if this local shadow pass
                // has no surviving detail instances. Retain the original draw
                // and per-instance cull unchanged whenever any can survive.
                if (cullThisPass && !GLDetailShadowCull::EagerSetupRequested(Core.Params))
                {
                    // m_visibles/matrices/scales stay immutable after Render's
                    // task wait throughout this package. Bound every possible
                    // wave phase/direction, not the first light's current wind.
                    const float amplitude = var_id == 1 ? swing_current.amp1 : swing_current.amp2;
                    const float windLimit = _abs(amplitude) * 1.001f;
                    const auto aggregate = GLDetailShadowCull::BatchBounds(this, var_id, O, vis.size(),
                        [&](GLDetailShadowCull::Bounds* cells)
                    {
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                        if (detailCullStats)
                            ++detailCullStats->modelBoundsBuilt;
#endif
                        Fvector4 envelope;
                        envelope.set(windLimit, 0, windLimit, 0);
                        const auto model = GLDetailShadowCull::BuildModelBounds(vis, cells, [&](const SlotItem& instance)
                        {
                            return GLDetailShadowCull::Build(Object.bv_bb, instance.mRotY,
                                instance.scale_calculated, envelope, wave, var_id != 0);
                        });
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                        if (detailCullStats && cells && model.valid)
                            detailCullStats->cellBoundsBuilt += unsigned(vis.size());
#endif
                        return model;
                    });
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    if (detailCullStats)
                        ++detailCullStats->modelTests;
#endif
                    if (detailLocalShadowPlanes->Outside(aggregate.envelope))
                    {
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                        if (detailCullStats)
                            ++detailCullStats->modelsRejected;
#endif
                        continue;
                    }
                    if (aggregate.cells &&
                        (var_id == 0 || GLDetailShadowCull::WindWithinEnvelope(windLimit, wind, wave)))
                        cellBounds = aggregate.cells;
                }
#endif
                // Setup matrices + colors (and flush it as necessary)
                //RCache.set_Element				(Object.shader->E[lod_id]);
                cmd_list.set_Element(element, iPass);
                cmd_list.apply_lmaterial();

                //	This could be cached in the corresponding consatant buffer
                //	as it is done for DX9
                cmd_list.set_c(strConsts, consts);
                cmd_list.set_c(strWave, wave);
                cmd_list.set_c(strDir2D, wind);
                cmd_list.set_c(strXForm, Device.mFullTransform);

                ref_constant constArray = cmd_list.get_c(strArray);
                if (!constArray)
                    constArray = cmd_list.get_c("array[0]");
                if (!constArray)
                    constArray = (lod_id == 0) ? hwc_array : hwc_s_array;

                R_constant* array_ptr = constArray._get();
                if (!array_ptr)
                    continue;
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
                const bool batchUniforms = GLUniformBatch::Enabled() &&
                    GLDetailUniformBatch::CanBatch(*array_ptr, hw_BatchSize);
                std::array<Fvector4, GLDetailUniformBatch::MaxInstances * GLDetailUniformBatch::RowsPerInstance> rows;
#endif

                //u32			c_base				= x_array->vs.index;
                //Fvector4*	c_storage			= RCache.get_ConstantCache_Vertex().get_array_f().access(c_base);
                // TODO: OGL: Implement a constants buffer.
                /*Fvector4*	c_storage=0;
                //	Map constants to memory directly
                {
                    void*	pVData;
                    RCache.get_ConstantDirect( strArray,
                        hw_BatchSize*sizeof(Fvector4)*4,
                        &pVData, 0, 0);
                    c_storage = (Fvector4*) pVData;
                }
                VERIFY(c_storage);*/

                u32 dwBatch = 0;

                for (auto items : vis)
                {
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
                    bool rejectCell = false;
                    if (cellBounds)
                    {
                        rejectCell = detailLocalShadowPlanes->Outside(*cellBounds++);
                    }
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    if (detailCullStats && cullThisPass)
                    {
                        detailCullStats->instanceCandidates += unsigned(items->size());
                        detailCullStats->cellsTested += cellBounds != nullptr;
                        if (rejectCell)
                        {
                            ++detailCullStats->cellsRejected;
                            detailCullStats->instancesSkipped += unsigned(items->size());
                        }
                    }
#endif
                    if (rejectCell)
                        continue;
#endif
                    for (auto& instance : *items)
                    {
#if defined(USE_OGL) && (defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES))
                        if (cullThisPass)
                        {
                            const auto bounds = GLDetailShadowCull::Build(Object.bv_bb, instance->mRotY,
                                instance->scale_calculated, wind, wave, var_id != 0);
                            if (detailLocalShadowPlanes->Outside(bounds))
                                continue;
                        }
#endif
                        u32 base = dwBatch * 4;

                        // Build matrix ( 3x4 matrix, last row - color )
                        float scale = instance->scale_calculated;
                        Fmatrix& M = instance->mRotY;
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
                        if (batchUniforms)
                            GLDetailUniformBatch::Pack(M, scale, instance->c_sun, instance->c_hemi, rows.data() + base);
                        else
#endif
                        {
                            cmd_list.set_ca(array_ptr, base + 0, M._11 * scale, M._21 * scale, M._31 * scale, M._41);
                            cmd_list.set_ca(array_ptr, base + 1, M._12 * scale, M._22 * scale, M._32 * scale, M._42);
                            cmd_list.set_ca(array_ptr, base + 2, M._13 * scale, M._23 * scale, M._33 * scale, M._43);

                            // Build color
                            // R2 only needs hemisphere
                            float h = instance->c_hemi;
                            float s = instance->c_sun;
                            cmd_list.set_ca(array_ptr, base + 3, s, s, s, h);
                        }
#if defined(XR_PLATFORM_ANDROID)
                        if (nativeR1DetailPass)
                        {
                            // Original R1 CPU instance color, not R2 sun/hemi channels.
                            Fvector color = c_ambient;
                            color.mad(c_hemi, instance->c_hemi);
                            color.mad(c_sun, instance->c_sun);
                            if (batchUniforms) rows[base+3].set(color.x,color.y,color.z,1.f);
                            else cmd_list.set_ca(array_ptr,base+3,color.x,color.y,color.z,1.f);
                        }
#endif
                        dwBatch ++;
                        if (dwBatch == hw_BatchSize)
                        {
                            // flush
                            RImplementation.BasicStats.DetailCount += dwBatch;
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                            GLGpuProfiler::RecordDetails(dwBatch);
#endif
                            u32 dwCNT_verts = dwBatch * Object.number_vertices;
                            u32 dwCNT_prims = dwBatch * Object.number_indices / 3;
                            //RCache.get_ConstantCache_Vertex().b_dirty				=	TRUE;
                            //RCache.get_ConstantCache_Vertex().get_array_f().dirty	(c_base,c_base+dwBatch*4);
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
                            if (batchUniforms)
                                cmd_list.set_ca(array_ptr, 0u, dwBatch * GLDetailUniformBatch::RowsPerInstance, rows.data());
#endif
                            cmd_list.Render(D3DPT_TRIANGLELIST, vOffset, 0, dwCNT_verts, iOffset, dwCNT_prims);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                            if (RImplementation.get_context(cmd_list.context_id).o.phase == CRender::PHASE_NORMAL)
                                GLGpuProfiler::RecordR1Material(element->passes[iPass]->ps->cName.c_str(),1,nativeR1DetailPass ? 1 : 0);
#endif
                            cmd_list.stat.r.s_details.add(dwCNT_verts);

                            // restart
                            dwBatch = 0;

                            //	Remap constants to memory directly (just in case anything goes wrong)
                            /*{
                                void*	pVData;
                                RCache.get_ConstantDirect( strArray,
                                    hw_BatchSize*sizeof(Fvector4)*4,
                                    &pVData, 0, 0);
                                c_storage = (Fvector4*) pVData;
                            }
                            VERIFY(c_storage);*/
                        }
                    }
                }
                // flush if nessecary
                if (dwBatch)
                {
                    RImplementation.BasicStats.DetailCount += dwBatch;
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    GLGpuProfiler::RecordDetails(dwBatch);
#endif
                    u32 dwCNT_verts = dwBatch * Object.number_vertices;
                    u32 dwCNT_prims = dwBatch * Object.number_indices / 3;
                    //RCache.get_ConstantCache_Vertex().b_dirty				=	TRUE;
                    //RCache.get_ConstantCache_Vertex().get_array_f().dirty	(c_base,c_base+dwBatch*4);
#if defined(XR_PLATFORM_ANDROID) || defined(XRAY_USE_GLES)
                    if (batchUniforms)
                        cmd_list.set_ca(array_ptr, 0u, dwBatch * GLDetailUniformBatch::RowsPerInstance, rows.data());
#endif
                    cmd_list.Render(D3DPT_TRIANGLELIST, vOffset, 0, dwCNT_verts, iOffset, dwCNT_prims);
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
                    if (RImplementation.get_context(cmd_list.context_id).o.phase == CRender::PHASE_NORMAL)
                        GLGpuProfiler::RecordR1Material(element->passes[iPass]->ps->cName.c_str(),1,nativeR1DetailPass ? 1 : 0);
#endif
                    cmd_list.stat.r.s_details.add(dwCNT_verts);
                }
            }
        }
        vOffset += hw_BatchSize * Object.number_vertices;
        iOffset += hw_BatchSize * Object.number_indices;
    }
}
} // namespace xray::render::RENDER_NAMESPACE
