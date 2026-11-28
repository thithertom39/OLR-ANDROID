#include "stdafx.h"
#include "Layers/xrRender/light.h"
#include "xrCDB/Intersect.hpp"
#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
#include "xrEngine/AndroidSceneCompare.h"
#endif

namespace xray::render::RENDER_NAMESPACE
{
const u32 delay_small_min = 1;
const u32 delay_small_max = 3;
const u32 delay_large_min = 10;
const u32 delay_large_max = 20;
const u32 cullfragments = 4;

void light::vis_prepare(CBackend& cmd_list)
{
    if (int(indirect_photons) != ps_r2_GI_photons)
        gi_generate();

    // Disabled queries always resolve as visible. Avoid a query-only volume
    // draw while preserving the normal lighting and shadow passes.
    if (!RImplementation.HWOCC.is_enabled())
    {
        vis.visible = true;
        vis.pending = false;
        vis.query_order = 0;
        return;
    }

    //	. test is sheduled for future	= keep old result
    //	. test time comes :)
    //		. camera inside light volume	= visible,	shedule for 'small' interval
    //		. perform testing				= ???,		pending

    u32 frame = Device.dwFrame;
    if (frame < vis.frame2test)
        return;

    float safe_area = VIEWPORT_NEAR;
    {
        float a0 = deg2rad(Device.fFOV * Device.fASPECT / 2.f);
        float a1 = deg2rad(Device.fFOV / 2.f);
        float x0 = VIEWPORT_NEAR / _cos(a0);
        float x1 = VIEWPORT_NEAR / _cos(a1);
        float c = _sqrt(x0 * x0 + x1 * x1);
        safe_area = _max(_max(VIEWPORT_NEAR, _max(x0, x1)), c);
    }

    // Msg	("sc[%f,%f,%f]/c[%f,%f,%f] - sr[%f]/r[%f]",VPUSH(spatial.center),VPUSH(position),spatial.radius,range);
    // Msg	("dist:%f, sa:%f",Device.vCameraPosition.distance_to(spatial.center),safe_area);
    bool skiptest = false;
    if (ps_r2_ls_flags.test(R2FLAG_EXP_DONT_TEST_UNSHADOWED) && !flags.bShadow)
        skiptest = true;
    if (ps_r2_ls_flags.test(R2FLAG_EXP_DONT_TEST_SHADOWED) && flags.bShadow)
        skiptest = true;

    if (skiptest || Device.vCameraPosition.distance_to(spatial.sphere.P) <= (spatial.sphere.R * 1.01f + safe_area))
    { // small error
        vis.visible = true;
        vis.pending = false;
        vis.frame2test = frame + ::Random.randI(delay_small_min, delay_small_max);
        return;
    }

    // testing
    vis.pending = true;
    xform_calc();
    cmd_list.set_xform_world(m_xform);
    vis.query_order = RImplementation.occq_begin(vis.query_id);
    //	Hack: Igor. Light is visible if it's frutum is visible. (Only for volumetric)
    //	Hope it won't slow down too much since there's not too much volumetric lights
    //	TODO: sort for performance improvement if this technique hurts
    if ((flags.type == IRender_Light::SPOT) && flags.bShadow && flags.bVolumetric)
        cmd_list.set_Stencil(FALSE);
    else
        cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0xff, 0x00);
    RImplementation.Target->draw_volume(cmd_list, this);
    RImplementation.occq_end(vis.query_id);
}

void light::vis_update()
{
    //	. not pending	->>> return (early out)
    //	. test-result:	visible:
    //		. shedule for 'large' interval
    //	. test-result:	invisible:
    //		. shedule for 'next-frame' interval

    if (!vis.pending)
        return;

    const u32 frame = Device.dwFrame;
    const auto fragments = RImplementation.occq_get(vis.query_id);
    // Log					("",fragments);
    vis.visible = (fragments > cullfragments);
    vis.pending = false;
    if (vis.visible)
    {
        vis.frame2test = frame + ::Random.randI(delay_large_min, delay_large_max);
    }
    else
    {
        vis.frame2test = frame + 1;
    }
}

#if defined(USE_OGL) && defined(XR_PLATFORM_ANDROID)
void R_occlusion::occq_light_issue(CBackend& cmd_list, const xr_vector<light*>& candidates)
{
    // Indirect and volumetric lighting may contribute outside this volume.
    // The non-MSAA deferred depth at this point is the accumulation depth.
    if (candidates.empty() || ps_r2_ls_flags.test(R2FLAG_GI))
        return;
    const auto& project = Device.mProject;
    if (project._34 != 1.f || project._44 != 0.f || project._33 <= 0.f)
        return;
    const float nearDepth = -project._43 / project._33;
    if (!_valid(nearDepth) || nearDepth <= 0.f || !occq_light_begin_frame())
        return;

    for (u32 index = 0; index < candidates.size(); ++index)
    {
        auto* source = candidates[index];
        const auto* owner = source->shadow_parent ? source->shadow_parent : source;
        if (owner->flags.bHudMode || source->flags.bVolumetric ||
            source->s_point || source->s_spot || source->s_volumetric ||
            !_valid(source->range) || source->range <= 0.f)
            continue;

        // Full front and back volume faces make the query conservative for
        // deferred lighting. If clipping could remove the front surface,
        // the camera may be in/near the volume: keep the light.
        float radius = source->range;
        if (source->flags.type == IRender_Light::SPOT)
            radius *= _sqrt(1.f + _sqr(tanf(source->cone * 0.5f)));
        else if (source->flags.type != IRender_Light::OMNIPART)
            continue;
        Fvector viewPosition;
        Device.mView.transform_tiny(viewPosition, source->position);
        if (!_valid(viewPosition.z) || !_valid(radius) ||
            viewPosition.z <= nearDepth + radius * 1.01f + EPS_L)
            continue;

        if (!occq_light_begin(index, source))
            break; // Remaining lights stay visible if the ring is full.
        if (!lightQueriesIssued)
        {
            RImplementation.rmNormal(cmd_list);
            cmd_list.set_xform_view(Device.mView);
            cmd_list.set_xform_project(project);
            cmd_list.set_CullMode(CULL_NONE);
            cmd_list.set_Scissor(nullptr);
            cmd_list.set_Stencil(FALSE);
            cmd_list.set_Z(true);
            cmd_list.set_ZFunc(D3DCMP_LESSEQUAL);
        }
        source->xform_calc();
        cmd_list.set_xform_world(source->m_xform);
        RImplementation.Target->draw_volume(cmd_list, source);
        occq_light_end();
    }

    if (lightQueriesIssued)
    {
        if (!lightQueryActive && AndroidSceneCompare::HasOption(Core.Params, "-gles_light_occq_flush"))
            CHK_GL(glFlush());
        cmd_list.set_CullMode(CULL_CCW);
        cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0xff, 0x00);
    }
}
#endif
} // namespace xray::render::RENDER_NAMESPACE
