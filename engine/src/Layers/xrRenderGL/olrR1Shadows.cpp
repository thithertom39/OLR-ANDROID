#include "stdafx.h"
#include "olrR1Shadows.h"
#if defined(XR_PLATFORM_ANDROID)
#include "xrEngine/IRenderable.h"
#include "Layers/xrRender/FBasicVisual.h"
#include "Layers/xrRender/LightTrack.h"
#include "Layers/xrRender/light.h"
#include "Layers/xrRender/Blender.h"
#include "Layers/xrRender/Blender_Recorder.h"
#include "olrR1Models.h"

namespace xray::render::RENDER_NAMESPACE::OlrR1Shadows
{
namespace
{
constexpr unsigned size=512,tile=85,line=size/tile,capacity=line*line;
struct Node { dxRender_Visual* visual; Fmatrix world; };
struct Caster { IRenderable* object; Fvector center; xr_vector<Node> nodes; };
struct Shadow { IRenderable* object; Fvector center; Fmatrix combined; light* source; float energy; unsigned slot; };
xr_vector<Caster> casters;
xr_vector<Shadow> shadows;
ref_rt raw,blurred;
ref_shader blur;
ref_geom quad;

class BlurMaterial final : public IBlender
{
public:
    LPCSTR getComment() override { return "R1 original four tap shadow blur"; }
    void Compile(CBlender_Compile& C) override
    {
        IBlender::Compile(C);
        if (C.iElement != 0) return;
        C.r_Pass("olr_r1_shadow_blur","olr_r1_shadow_blur",FALSE,FALSE,FALSE,FALSE);
        C.r_Sampler_clf("s_base","$user$olr_r1_shadow_raw");
        C.r_End();
    }
};

void EnsureResources()
{
    if (raw) return;
    raw.create("$user$olr_r1_shadow_raw",size,size,D3DFMT_A8R8G8B8);
    blurred.create("$user$shadow",size,size,D3DFMT_A8R8G8B8);
    BlurMaterial material;
    blur.create(&material,"olr_r1_shadow_blur");
    quad.create(FVF::F_TL,RImplementation.Vertex.Buffer(),RImplementation.QuadIB);
    Msg("* [olr_r1_shadows] original atlas RGBA8=%u tile=%u capacity=%u blur_kernel=.75; no R2 atlas",size,tile,capacity);
}

void Capture(const char* stage)
{
    if (!Core.Params || !strstr(Core.Params,"-olr_r1_shadow_capture")) return;
    xr_vector<u8> pixels(size*size*4);
    CHK_GL(glReadPixels(0,0,size,size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data()));
    string_path path,name;
    xr_sprintf(name,"r1-shadow-%s-%llu.rgba",stage,static_cast<unsigned long long>(CPU::QPC()));
    FS.update_path(path,"$logs$",name);
    auto* output=FS.w_open(path);
    if (output)
    {
        output->w_u32(size); output->w_u32(size);
        output->w(pixels.data(),pixels.size()); FS.w_close(output);
        Msg("* [olr_r1_shadows] isolated caster atlas capture=%s; world receiver projection not yet integrated",path);
    }
}
}

void BeginCollect() { casters.clear(); shadows.clear(); }

void Collect(IRenderable* object, dxRender_Visual* visual, const Fmatrix& world)
{
    if (!object || object->renderable_HUD() || !object->renderable_ShadowGenerate() ||
        !object->GetRenderData().visual || !visual->shader->olr_r1_model || !visual->shader->E[SE_R1_LMODELS]) return;
    auto* ros=static_cast<CROS_impl*>(object->renderable_ROS());
    if (!ros) return;
    const auto& root=object->GetRenderData().visual->getVisData();
    Fvector center; object->GetRenderData().xform.transform_tiny(center,root.sphere.P);
    const float distance=center.distance_to(Device.vCameraPosition)+root.sphere.R;
    if ((distance/48.f)*(4.f/(root.sphere.R+EPS)) >= 1.f) return;
    for (auto& caster : casters)
        if (caster.object == object)
        {
            for (const auto& node : caster.nodes)
                if (node.visual == visual && memcmp(&node.world,&world,sizeof(Fmatrix)) == 0) return;
            caster.nodes.push_back({visual,world}); return;
        }
    ros->shadow_gen_frame=Device.dwFrame;
    casters.push_back({object,center,{{visual,world}}});
}

void Calculate()
{
    if (casters.empty()) return;
    auto& commands=RCache;
    unsigned draws=0;
    bool bound=false;
    for (const auto& caster : casters)
    {
        auto* ros=static_cast<CROS_impl*>(caster.object->renderable_ROS());
        ros->update(caster.object);
        const float radius=caster.object->GetRenderData().visual->getVisData().sphere.R;
        for (const auto& light : ros->lights)
        {
            if (light.energy < .05f || shadows.size() >= capacity) continue;
            Fvector origin=light.source->position;
            float range=light.source->range;
            if (light.source->flags.type == IRender_Light::DIRECT)
            { origin.mul(light.source->direction,-100.f).add(caster.center); range=120.f; }
            else
            {
                float distance;
                while ((distance=caster.center.distance_to(origin)) <= EPS_L) origin.y+=.01f;
                if (distance < radius+.1f)
                {
                    Fvector direction; direction.sub(caster.center,origin).normalize();
                    origin.mad(origin,direction,distance-radius-.1f);
                }
            }
            const float distance=caster.center.distance_to(origin),hat=radius/distance;
            const float nearPlane=distance-radius-2.f*EPS_L;
            const float farPlane=_min(range,_max(distance+4.5f,distance+radius));
            if (nearPlane < 2.f*EPS_L || farPlane < nearPlane+2.f*EPS_L || hat > .9f || hat < .01f) continue;
            Fmatrix project,view,combined;
            project.build_projection_HAT(hat,1.f,nearPlane,farPlane);
            Fvector direction,up,right;
            direction.sub(caster.center,origin).normalize();
            up.set(0,1,0); if (1.f-_abs(direction.y) < EPS) up.set(1,0,0);
            right.crossproduct(up,direction); up.crossproduct(direction,right);
            view.build_camera(origin,caster.center,up);
            combined.mul(project,view);
            if (!bound)
            {
                EnsureResources();
                RImplementation.Target->u_setrt(commands,raw,nullptr,nullptr,ref_rt{});
                commands.set_Scissor(nullptr); commands.set_Stencil(FALSE);
                commands.ClearRT(raw,color_rgba(255,255,255,255));
                bound=true;
            }
            commands.set_xform_project(project); commands.set_xform_view(view);
            const unsigned slot=unsigned(shadows.size()),x=(slot%line)*tile,y=(slot/line)*tile;
            commands.SetViewport({float(x),float(y),float(tile),float(tile),0.f,1.f});
            for (const auto& node : caster.nodes)
            {
                commands.set_Element(node.visual->shader->E[SE_R1_LMODELS]);
                commands.set_CullMode(CULL_CW); // caster atlas VS reverses Y
                commands.set_xform_world(node.world);
                const unsigned before=commands.stat.render.calls;
                node.visual->Render(commands,-1.f,false);
                draws+=commands.stat.render.calls-before;
            }
            shadows.push_back({caster.object,caster.center,combined,light.source,light.energy,slot});
        }
    }
    if (bound)
    {
        Capture("raw");
        RImplementation.Target->u_setrt(commands,blurred,nullptr,nullptr,ref_rt{});
        commands.SetViewport({0.f,0.f,float(size),float(size),0.f,1.f});
        commands.set_Shader(blur); commands.set_CullMode(CULL_NONE);
        u32 offset;
        auto* vertices=static_cast<FVF::TL*>(RImplementation.Vertex.Lock(4,quad->vb_stride,offset));
        vertices[0].set(-1,-1,0,1,0xffffffff,0,0);
        vertices[1].set(-1, 1,0,1,0xffffffff,0,1);
        vertices[2].set( 1,-1,0,1,0xffffffff,1,0);
        vertices[3].set( 1, 1,0,1,0xffffffff,1,1);
        RImplementation.Vertex.Unlock(4,quad->vb_stride);
        commands.set_Geometry(quad);
        commands.Render(D3DPT_TRIANGLELIST,offset,0,4,0,2);
        Capture("blurred");
        commands.set_xform_project(Device.mProject); commands.set_xform_view(Device.mView);
        commands.set_xform_world(Fidentity); commands.set_CullMode(CULL_CCW);
        RImplementation.Target->u_setrt(commands,Device.dwWidth,Device.dwHeight,
            RImplementation.Target->get_base_rt(),0,0,RImplementation.Target->get_base_zb());
        RImplementation.rmNormal(commands);
    }
    static unsigned reports=0;
    if (reports++ < 8)
        Msg("* [olr_r1_shadows] casters=%u slots=%u caster_draws=%u; receiver collision clipping/projection pending",
            unsigned(casters.size()),unsigned(shadows.size()),draws);
}

void Destroy()
{
    casters.clear(); shadows.clear(); quad.destroy(); blur.destroy(); raw.destroy(); blurred.destroy();
}
}
#endif
