#include "stdafx.h"

#if defined(XR_PLATFORM_ANDROID)
#include "Layers/xrRender/LightTrack.h"
#include "Layers/xrRender/xrRender_console.h"
#include "Include/xrRender/RenderVisual.h"
#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/Environment.h"
#include "xrEngine/xr_object.h"

namespace xray::render::RENDER_NAMESPACE
{
namespace
{
// OpenXRay/xray d7b2359, trunk/xr_3da/xrRender/LightTrack.cpp.
// Keep the original order: it determines the first frames' sky sampling.
constexpr float r1SkyDirections[lt_hemisamples][3] = {
    {0,1,0}, {.52573f,.85065f,0}, {.16246f,.85065f,.5f},
    {-.42533f,.85065f,.30902f}, {-.42533f,.85065f,-.30902f}, {.16246f,.85065f,-.5f},
    {.89443f,.44721f,0}, {.27639f,.44721f,.85065f}, {-.72361f,.44721f,.52573f},
    {-.72361f,.44721f,-.52573f}, {.27639f,.44721f,-.85065f},
    {.68819f,.52573f,.5f}, {-.26287f,.52573f,.80902f}, {-.85065f,.52573f,0},
    {-.26287f,.52573f,-.80902f}, {.68819f,.52573f,-.5f},
    {.95106f,0,.30902f}, {.58779f,0,.80902f}, {0,0,1}, {-.58779f,0,.80902f},
    {-.95106f,0,.30902f}, {-.95106f,0,-.30902f}, {-.58779f,0,-.80902f},
    {0,0,-1}, {.58779f,0,-.80902f}, {.95106f,0,-.30902f}
};
}

void CROS_impl::update_original_r1(IRenderable* object)
{
    if (dwFrame == Device.dwFrame) return;
    dwFrame = Device.dwFrame;
    if (!object || !object->GetRenderData().visual) return;
    auto* gameObject = dynamic_cast<IGameObject*>(object);
    const auto& sphere = object->GetRenderData().visual->getVisData().sphere;
    Fvector position;
    object->GetRenderData().xform.transform_tiny(position, sphere.P);
    position.y += .3f * sphere.R;
    auto* sun = static_cast<light*>(RImplementation.Lights.sun._get());
    if (!sun) return;

    if ((MODE & TRACE_SUN) && --result_sun < 0)
    {
        result_sun += Random.randI(lt_hemisamples/4, lt_hemisamples/2);
        Fvector direction = sun->direction;
        direction.invert().normalize();
        sun_value = !g_pGameLevel->ObjectSpace.RayTest(position, direction, 500.f,
            collide::rqtBoth, &cache_sun, gameObject) ? 1.f : 0.f;
    }
    const bool firstTime = result_count == 0;
    if (MODE & TRACE_HEMI)
        for (int i = 0; i < ps_r2_dhemi_count; ++i)
        {
            const unsigned sample = result_count < lt_hemisamples ? result_count++ : result_iterator++ % lt_hemisamples;
            Fvector direction;
            direction.set(r1SkyDirections[sample][0], r1SkyDirections[sample][1], r1SkyDirections[sample][2]).normalize();
            result[sample] = !g_pGameLevel->ObjectSpace.RayTest(position, direction, 50.f,
                collide::rqtStatic, &cache[sample], gameObject);
        }
    unsigned skyVisible = 0;
    for (int i = 0; i < result_count; ++i) skyVisible += unsigned(result[i]);
    hemi_value = float(skyVisible) / float(result_count ? result_count : 1) * ps_r1_dhemi_scale;
    if (firstTime) hemi_smooth = hemi_value;
    update_smooth(); // Original order: smooth sky/sun before computing light/average.

    const bool traceLights = (MODE & TRACE_LIGHTS) &&
        (object->renderable_ShadowGenerate() || object->renderable_ShadowReceive());
    if (traceLights)
    {
        // This query deliberately uses actual level/dynamic lights, not R2's
        // build.lights STYPE_LIGHTSOURCEHEMI and polynomial attenuation.
        xr_vector<ISpatial*> sources;
        Fvector halfSize;
        halfSize.set(sphere.R, sphere.R, sphere.R);
        g_pGamePersistent->SpatialSpace.q_box(sources, 0, STYPE_LIGHTSOURCE, position, halfSize);
        for (auto* spatial : sources)
        {
            auto* source = static_cast<light*>(spatial->dcast_Light());
            if (source && position.distance_to(source->position) < sphere.R + source->range)
                add(source);
        }
        lights.clear();
        for (size_t i = 0; i < track.size(); )
        {
            auto& item = track[i];
            if (item.frame_touched != Device.dwFrame)
            {
                track.erase(track.begin()+i);
                continue;
            }
            light* source = item.source;
            Fvector point, direction;
            point.random_dir();
            point.mad(position, point, sphere.R*.5f);
            const float distance = direction.sub(point, source->position).magnitude();
            // Coincident endpoints need no ray; avoid normalizing a zero vector.
            const bool occluded = distance > EPS && g_pGameLevel->ObjectSpace.RayTest(
                source->position, direction.div(distance), distance, collide::rqtStatic, &item.cache, gameObject);
            item.test = clampr(item.test + (occluded ? -lt_dec : lt_inc)*Device.fTimeDelta, -.5f, 1.f);
            item.energy = .9f*item.energy + .1f*item.test;
            if (item.energy*source->color.intensity() > EPS)
            {
                Light selected;
                selected.source = source;
                selected.color.mul_rgb(source->color, item.energy/2.f);
                selected.energy = item.energy/2.f;
                if (!source->flags.bStatic)
                {
                    selected.color.mul_rgb(.5f);
                    selected.energy *= .5f;
                }
                lights.push_back(selected);
            }
            ++i;
        }
        if (sun_smooth*sun->color.intensity() > EPS)
        {
            Light selected;
            selected.source = sun;
            selected.color.mul_rgb(sun->color, sun_smooth/2.f);
            selected.energy = sun_smooth;
            lights.push_back(selected);
        }
        std::sort(lights.begin(), lights.end(), [](const Light& a, const Light& b) { return a.energy > b.energy; });
    }
    const auto& environment = g_pGamePersistent->Environment().CurrentEnv;
    Fvector hemi, direct, average = environment.ambient;
    hemi.set(environment.hemi_color.x, environment.hemi_color.y, environment.hemi_color.z);
    hemi.mul(MODE & TRACE_HEMI ? hemi_smooth : .2f);
    direct.set(environment.sun_color).mul(MODE & TRACE_SUN ? sun_smooth : .2f);
    average.add(hemi).add(direct);
    if (MODE & TRACE_LIGHTS)
    {
        for (const auto& selected : lights)
        {
            const auto* source = selected.source;
            const float attenuation = clampr(1.f-position.distance_to(source->position)/(source->range+EPS), 0.f, 1.f) *
                (source->flags.bStatic ? 1.f : 2.f);
            average.x += selected.color.r*attenuation;
            average.y += selected.color.g*attenuation;
            average.z += selected.color.b*attenuation;
        }
    }
    else average.set(.1f, .1f, .1f); // Preserved original TRACE_LIGHTS-off behavior.
    approximate = average;
    static unsigned logged = 0;
    if (logged++ < 8)
        Msg("* [olr_r1_ros] original=1 samples=%d hemi=%f sun=%f lights=%u average=%f,%f,%f",
            result_count, hemi_smooth, sun_smooth, unsigned(lights.size()), average.x, average.y, average.z);
}
}
#endif
