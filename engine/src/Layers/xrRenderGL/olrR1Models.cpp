#include "stdafx.h"
#include "olrR1Models.h"

#if defined(XR_PLATFORM_ANDROID)
#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/IRenderable.h"
#include "Layers/xrRender/FBasicVisual.h"
#include "Layers/xrRender/LightTrack.h"
#include "Layers/xrRender/Blender_Recorder.h"
#include "Layers/xrRender/xrRender_console.h"
#include <mutex>

namespace xray::render::RENDER_NAMESPACE::OlrR1Models
{
namespace
{
constexpr unsigned atlasSize = 512, tileSize = 51, tilesPerLine = atlasSize / tileSize;
constexpr unsigned slotCount = tilesPerLine * tilesPerLine;
constexpr float clipDistance = 50.f, cameraHeight = 200.f, cameraRange = 7.f;
struct Slot
{
    IRenderable* object = nullptr;
    Fvector center{}, clampMin{}, clampMax{};
    Fmatrix uv{};
    Fbox bounds{};
    u32 frame = u32(-1), expires = 0;
};
struct Projector : pureAppActivate
{
    ref_rt atlas, depth;
    std::array<Slot, slotCount> cache{};
    xr_vector<IRenderable*> receivers;
    xr_map<IRenderable*,ISpatial*> spatialOwners;
    std::mutex spatialMutex;
    bool registered = false;
    void OnAppActivate() override
    {
        for (auto& slot : cache) slot.expires = 0;
    }
};
Projector projector;

void EnsureResources()
{
    if (projector.atlas) return;
    projector.atlas.create("$user$projector", atlasSize, atlasSize, D3DFMT_A8R8G8B8);
    projector.depth.create("$user$olr_r1_projector_depth", atlasSize, atlasSize, D3DFMT_D24S8);
    Device.seqAppActivate.Add(&projector);
    projector.registered = true;
    Msg("* [olr_r1_projector] created original RGBA8 atlas=%u tile=%u slots=%u; no R2 shadow atlas", atlasSize, tileSize, slotCount);
}

void Capture()
{
    static bool captured = false;
    if (captured || !Core.Params || !strstr(Core.Params, "-olr_r1_projector_capture")) return;
    captured = true;
    xr_vector<u8> pixels(atlasSize*atlasSize*4);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    CHK_GL(glReadPixels(0, 0, atlasSize, atlasSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
    string_path name, path;
    xr_sprintf(name, "r1-projector-%llu.rgba", static_cast<unsigned long long>(CPU::QPC()));
    FS.update_path(path, "$logs$", name);
    IWriter* output = FS.w_open(path);
    if (output)
    {
        output->w_u32(atlasSize); output->w_u32(atlasSize);
        output->w(pixels.data(), pixels.size());
        FS.w_close(output);
        Msg("* [olr_r1_projector] captured %s", path);
    }
}
}

void BeginCollect()
{
    projector.receivers.clear();
    std::lock_guard<std::mutex> lock(projector.spatialMutex);
    projector.spatialOwners.clear();
}

void Observe(IRenderable* object, ISpatial* spatial)
{
    // The visibility query already provides this exact association through
    // ISpatial::dcast_Renderable. Cross-library RTTI fails for these Android
    // game objects; do not replace their real sector by an invented one.
    std::lock_guard<std::mutex> lock(projector.spatialMutex);
    projector.spatialOwners[object]=spatial;
}

ISpatial* SpatialOf(IRenderable* object)
{
    std::lock_guard<std::mutex> lock(projector.spatialMutex);
    const auto found=projector.spatialOwners.find(object);
    return found!=projector.spatialOwners.end() ? found->second : dynamic_cast<ISpatial*>(object);
}
unsigned Receivers() { return unsigned(projector.receivers.size()); }

bool Collect(IRenderable* object)
{
    static const bool diagnose = Core.Params && strstr(Core.Params,"-olr_r1_projector_capture");
    static unsigned reports = 0;
    if (diagnose && reports++ < 12)
    {
        auto* visual=object ? object->GetRenderData().visual : nullptr;
        Fvector center; center.set(0,0,0);
        if (visual) object->GetRenderData().xform.transform_tiny(center,visual->getVisData().sphere.P);
        const float radius=visual ? visual->getVisData().sphere.R : 0;
        auto* spatial=SpatialOf(object);
        Msg("* [olr_r1_projector_select] root=%u visual=%u receive=%u invisible=%u R=%f D=%f limit=%f spatial=%u sector=%u camera=%f,%f,%f frame=%u",
            unsigned(object!=nullptr),unsigned(visual!=nullptr),unsigned(object && object->renderable_ShadowReceive()),
            unsigned(object && object->renderable_Invisible()),radius,center.distance_to(Device.vCameraPosition)+radius,
            clipDistance*radius,unsigned(spatial!=nullptr),spatial ? spatial->GetSpatialData().sector_id : u32(-1),
            Device.vCameraPosition.x,Device.vCameraPosition.y,Device.vCameraPosition.z,Device.dwFrame);
    }
    if (!object || !object->GetRenderData().visual || !object->renderable_ShadowReceive() || object->renderable_Invisible())
        return false;
    auto* ros = static_cast<CROS_impl*>(object->renderable_ROS());
    if (!ros) return false;
    if (ros->shadow_recv_frame == Device.dwFrame)
        return std::find(projector.receivers.begin(), projector.receivers.end(), object) != projector.receivers.end();
    if (projector.receivers.size() >= slotCount) return false; // Original hardware atlas capacity, not a light cap.
    const auto& sphere = object->GetRenderData().visual->getVisData().sphere;
    Fvector center;
    object->GetRenderData().xform.transform_tiny(center, sphere.P);
    if (!_valid(sphere.R) || sphere.R*1.1f <= EPS_L || center.distance_to(Device.vCameraPosition)+sphere.R >= clipDistance*sphere.R)
        return false;
    auto* spatial = SpatialOf(object);
    if (!spatial) return false;
    auto& graph = RImplementation.get_imm_context();
    if (spatial->GetSpatialData().type & STYPEFLAG_INVALIDSECTOR)
        spatial->spatial_updatesector(graph.detect_sector(spatial->spatial_sector_point()));
    if (spatial->GetSpatialData().sector_id == IRender_Sector::INVALID_SECTOR_ID) return false;
    // A newly created ROS has approximate=(0,0,0). The legacy one-object
    // visibility update may not have visited it yet. Do the original initial
    // ray/light update before caching its lighting for 30-90 seconds, rather
    // than seeding a persistent black tile and updating ROS only at model draw.
    if (ros->result_count == 0) ros->update(object);
    ros->shadow_recv_frame = Device.dwFrame;
    projector.receivers.push_back(object);
    return true;
}

void CalculateProjector()
{
    if (projector.receivers.empty()) return;
    EnsureResources();
    xr_vector<unsigned> tasks;
    unsigned reused = 0;
    for (unsigned i = 0; i < projector.receivers.size(); ++i)
    {
        auto* object = projector.receivers[i];
        auto* ros = static_cast<CROS_impl*>(object->renderable_ROS());
        const int index = ros->shadow_recv_slot;
        Fbox bounds;
        bounds.xform(object->GetRenderData().visual->getVisData().box, object->GetRenderData().xform);
        if (index >= 0 && index < int(slotCount) && projector.cache[index].object == object &&
            projector.cache[index].bounds.contains(bounds) && Device.dwTimeGlobal <= projector.cache[index].expires)
        {
            projector.cache[index].frame = Device.dwFrame;
            ++reused;
        }
        else tasks.push_back(i);
    }
    if (tasks.empty()) return;
    auto& commands = RCache;
    auto& graph = RImplementation.get_imm_context();
    RImplementation.Target->u_setrt(commands, projector.atlas, nullptr, nullptr, projector.depth);
    commands.set_Scissor(nullptr);
    commands.ClearZB(projector.depth, 1.f);
    commands.set_xform_world(Fidentity);
    commands.set_Stencil(FALSE);
    // Atlas VS flips Y to reproduce D3D9 row order; reverse face winding too.
    commands.set_CullMode(CULL_CW);
    const unsigned startDraws = commands.stat.render.calls;
    unsigned updated = 0;
    for (unsigned i = 0; i < slotCount && !tasks.empty(); ++i)
    {
        auto& slot = projector.cache[i];
        if (slot.frame == Device.dwFrame) continue;
        auto* object = projector.receivers[tasks.back()]; tasks.pop_back();
        auto* ros = static_cast<CROS_impl*>(object->renderable_ROS());
        const auto& visual = object->GetRenderData().visual->getVisData();
        slot.object = object;
        object->GetRenderData().xform.transform_tiny(slot.center, visual.sphere.P);
        slot.center.y += visual.sphere.R*.1f;
        slot.bounds.xform(visual.box, object->GetRenderData().xform).scale(.1f);
        slot.expires = Device.dwTimeGlobal + Random.randI(30000, 90000);
        slot.frame = Device.dwFrame;
        ros->shadow_recv_slot = i;
        const float radius = visual.sphere.R*1.1f;
        Fmatrix projection, view, combined;
        projection.build_projection_HAT(radius/cameraHeight, 1.f, cameraHeight-EPS_L, cameraHeight+radius+cameraRange);
        Fvector eye = slot.center, up;
        eye.y += cameraHeight; up.set(0,0,1);
        view.build_camera(eye, slot.center, up);
        commands.set_xform_project(projection);
        commands.set_xform_view(view);
        const unsigned x = (i % tilesPerLine)*tileSize, y = (i / tilesPerLine)*tileSize;
        commands.SetViewport({float(x),float(y),float(tileSize),float(tileSize),0.f,1.f});
        // CBackend's general scissor uses Device.dwHeight; this atlas has its
        // own height. Direct GLES coordinates prevent clearing adjacent slots.
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, tileSize, tileSize);
        const auto& average = ros->get_approximate();
        commands.ClearRT(projector.atlas, Fcolor().set(average.x,average.y,average.z,(average.x+average.y+average.z)/4.f));
        combined.mul(projection, view);
        const float halfTexel = .5f/atlasSize, slotSize = float(tileSize)/atlasSize;
        Fmatrix adjustment;
        adjustment.identity();
        adjustment._11=.5f; adjustment._22=-.5f;
        adjustment._41=.5f; adjustment._42=.5f+halfTexel;
        Fmatrix transform;
        slot.uv.mul(adjustment, combined);
        transform.scale(slotSize, slotSize, 1);
        slot.uv.mulA_44(transform);
        transform.translate(float(x)/atlasSize+halfTexel,float(y)/atlasSize+halfTexel,0);
        slot.uv.mulA_44(transform);
        Fbox box;
        box.vMin.set(slot.center.x-radius,slot.center.y-radius-cameraRange,slot.center.z-radius);
        box.vMax.set(slot.center.x+radius,slot.center.y,slot.center.z+radius);
        slot.clampMin.set(box.vMin).add(.05f);
        slot.clampMax.set(box.vMax).sub(.05f);
        auto* spatial = SpatialOf(object);
        graph.render_R1_box(spatial->GetSpatialData().sector_id, box, SE_R1_LMODELS);
        ++updated;
    }
    glDisable(GL_SCISSOR_TEST);
    Capture();
    commands.set_CullMode(CULL_CCW);
    commands.set_xform_project(Device.mProject);
    commands.set_xform_view(Device.mView);
    RImplementation.Target->u_setrt(commands, Device.dwWidth, Device.dwHeight,
        RImplementation.Target->get_base_rt(),0,0,RImplementation.Target->get_base_zb());
    RImplementation.rmNormal(commands);
    static unsigned reports = 0;
    if (reports++ < 8)
        Msg("* [olr_r1_projector] receivers=%u updated=%u reused=%u lighting_surface_draws=%u pending=%u",
            Receivers(),updated,reused,commands.stat.render.calls-startDraws,unsigned(tasks.size()));
}

void Bind(CBackend& commands, IRenderable* object, dxRender_Visual* visual)
{
    if (!visual || !visual->shader->olr_r1_model) return;
    commands.set_c("olr_r1_alpha_ref",float(visual->shader->olr_r1_alpha_ref),0.f,0.f,0.f);
    if (!object || !object->renderable_ROS()) return;
    auto& ros = *static_cast<CROS_impl*>(object->renderable_ROS());
    ros.update_smooth(object);
    const float sun = .5f*ros.get_sun();
    commands.set_c("olr_r1_model_props",sun,sun,sun,.5f*ros.get_hemi());
    const int index = ros.shadow_recv_slot;
    if (ros.shadow_recv_frame != Device.dwFrame || index < 0 || index >= int(slotCount)) return;
    const auto& slot = projector.cache[index];
    if (slot.object != object || slot.frame != Device.dwFrame) return;
    const float radius = object->GetRenderData().visual->getVisData().sphere.R;
    const float distance = slot.center.distance_to(Device.vCameraPosition)+radius;
    const float factor = _sqr(distance/(clipDistance*radius))*(1.f-ps_r1_lmodel_lerp)+ps_r1_lmodel_lerp;
    commands.set_c("olr_r1_model_projector",slot.uv);
    commands.set_ca("olr_r1_model_clamp",0,slot.clampMin.x,slot.clampMin.y,slot.clampMin.z,factor);
    commands.set_ca("olr_r1_model_clamp",1,slot.clampMax.x,slot.clampMax.y,slot.clampMax.z,0.f);
}

void Destroy()
{
    if (projector.registered) Device.seqAppActivate.Remove(&projector);
    projector.registered = false;
    projector.atlas.destroy(); projector.depth.destroy();
    projector.receivers.clear();
    { std::lock_guard<std::mutex> lock(projector.spatialMutex); projector.spatialOwners.clear(); }
    for (auto& slot : projector.cache) slot = Slot{};
}

void CaptureModels(R_dsgraph_structure& graph)
{
    static bool captured = false;
    if (captured || !Core.Params || !strstr(Core.Params,"-olr_r1_projector_capture")) return;
    captured = true;
    // One diagnostic image, never presented as a playable/complete world.
    // Do not consume queues: coverage still rejects unported LightPPA/shadows.
    auto& commands = RCache;
    auto* target = RImplementation.Target;
    target->u_setrt(commands,target->rt_Generic_1,nullptr,nullptr,target->rt_Base_Depth);
    RImplementation.rmNormal(commands);
    commands.ClearRT(target->rt_Generic_1,color_rgba(0,0,0,0));
    commands.ClearZB(target->rt_Base_Depth,1.f);
    commands.set_xform_view(Device.mView);
    commands.set_xform_project(Device.mProject);
    const unsigned before = commands.stat.render.calls;
    unsigned hq=0,lq=0;
    for (unsigned pass=0;pass<SHADER_PASSES_MAX;++pass)
    {
        xr_vector<R_dsgraph::mapMatrix_T::value_type*> groups;
        graph.mapMatrixPasses[0][pass].get_any_p(groups);
        for (auto* group:groups)
            for (auto& item:group->second)
            {
                if (!item.pObject || !item.pVisual->shader->olr_r1_model) continue;
                commands.set_Pass(group->first);
                commands.set_xform_world(item.Matrix);
                RImplementation.apply_object(commands,item.pObject);
                Bind(commands,item.pObject,item.pVisual);
                item.pVisual->Render(commands,1.f,false);
                auto* ros=static_cast<CROS_impl*>(item.pObject->renderable_ROS());
                if (ros && ros->shadow_recv_frame==Device.dwFrame) ++hq; else ++lq;
            }
    }
    xr_vector<u8> pixels(Device.dwWidth*Device.dwHeight*4);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    CHK_GL(glReadPixels(0,0,Device.dwWidth,Device.dwHeight,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data()));
    string_path name,path;
    xr_sprintf(name,"r1-models-%llu.rgba",static_cast<unsigned long long>(CPU::QPC()));
    FS.update_path(path,"$logs$",name);
    IWriter* output=FS.w_open(path);
    if (output)
    {
        output->w_u32(Device.dwWidth);output->w_u32(Device.dwHeight);
        output->w(pixels.data(),pixels.size());FS.w_close(output);
        Msg("* [olr_r1_models] diagnostic=%s HQ=%u LQ=%u draws=%u; local light and shadow integration pending",
            path,hq,lq,commands.stat.render.calls-before);
    }
    commands.set_xform_world(Fidentity);
    target->u_setrt(commands,Device.dwWidth,Device.dwHeight,target->get_base_rt(),0,0,target->get_base_zb());
}

void Compile(CBlender_Compile& compiler, bool blend, unsigned alphaRef)
{
    // The authored reference is bound per visual (including shared programs).
    (void)alphaRef;
    const bool local = compiler.iElement == SE_R1_LPOINT || compiler.iElement == SE_R1_LSPOT;
    pcstr stem = nullptr;
    switch (compiler.iElement)
    {
    case SE_R1_NORMAL_HQ: stem = "olr_r1_model_hq"; break;
    case SE_R1_NORMAL_LQ: stem = "olr_r1_model_lq"; break;
    case SE_R1_LPOINT: stem = "olr_r1_model_point"; break;
    case SE_R1_LSPOT: stem = "olr_r1_model_spot"; break;
    // The original projected shadow caster is added with LightShadows. Do
    // not substitute a R2 shadow-map shader for SE_R1_LMODELS in this mode.
    default: return;
    }
    string128 program;
    xr_sprintf(program,"%s%s",stem,blend ? "_aref" : "");
    if (local)
    {
        compiler.r_Pass(program,program,FALSE,TRUE,FALSE,TRUE,D3DBLEND_ONE,D3DBLEND_ONE);
        compiler.RS.SetRS(D3DRS_ZFUNC,D3DCMP_EQUAL);
        compiler.r_ColorWriteEnable(true,true,true,false);
    }
    else
        compiler.r_Pass(program,program,FALSE,TRUE,TRUE,blend,D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA);
    compiler.r_Sampler("s_base",compiler.L_textures[0]);
    if (compiler.iElement == SE_R1_NORMAL_HQ)
        compiler.r_Sampler_clf("s_lmap","$user$projector");
    if (local)
    {
        const bool spot = compiler.iElement == SE_R1_LSPOT;
        compiler.r_Sampler_clf("s_lmap",spot ? "internal\\internal_light_att" : "internal\\internal_light_attpoint");
        compiler.r_Sampler_clf("s_r1_att",spot ? "internal\\internal_light_attclip" : "internal\\internal_light_attpoint");
    }
    compiler.r_End();
}
}
#endif
