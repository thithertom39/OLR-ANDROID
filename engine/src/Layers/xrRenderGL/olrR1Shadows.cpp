#include "stdafx.h"
#include "olrR1Shadows.h"
#if defined(XR_PLATFORM_ANDROID)
#include "xrEngine/IRenderable.h"
#include "xrEngine/xr_object.h"
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
ref_shader blur,worldMaterial;
ref_geom quad,worldGeometry;
struct Triangle { Fvector p[3],normal; };
struct CacheEntry
{
    Fvector objectPosition,lightPosition;
    u32 time=0;
    bool valid=false;
    xr_vector<Triangle> triangles;
};
xr_map<std::pair<IRenderable*,light*>,CacheEntry> cache;
xr_vector<FVF::LIT> receivers;
xr_vector<light*> fixtureSources;
xrXRC collision("r1 shadow receivers");

class WorldMaterial final : public IBlender
{
public:
    LPCSTR getComment() override { return "R1 original multiplicative projected shadow"; }
    void Compile(CBlender_Compile& C) override
    {
        IBlender::Compile(C);
        if (C.iElement != 0) return;
        C.r_Pass("olr_r1_shadow_world","olr_r1_shadow_world",FALSE,TRUE,FALSE,TRUE,D3DBLEND_DESTCOLOR,D3DBLEND_ZERO);
        // Original fixed-function StageBegin defaults to WRAP, not CLAMP.
        C.r_Sampler("s_base","$user$shadow",false,D3DTADDRESS_WRAP,D3DTEXF_LINEAR,D3DTEXF_NONE,D3DTEXF_LINEAR);
        C.r_End();
    }
};

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
    WorldMaterial world;
    worldMaterial.create(&world,"olr_r1_shadow_world");
    worldGeometry.create(FVF::F_LIT,RImplementation.Vertex.Buffer(),0);
    Msg("* [olr_r1_shadows] original atlas RGBA8=%u tile=%u capacity=%u blur_kernel=.75; no R2 atlas",size,tile,capacity);
}

void Capture(const char* stage)
{
    if (!Core.Params || !strstr(Core.Params,"-olr_r1_shadow_capture")) return;
    static xr_set<xr_string> captured;
    if (!captured.insert(stage).second) return;
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
        Msg("* [olr_r1_shadows] isolated caster atlas capture=%s; not a full scene parity capture",path);
    }
}

int Intensity(const Fvector& position, const Fvector& normal, const Shadow& shadow)
{
    const auto* source=shadow.source;
    const float energy=source->color.intensity()*shadow.energy;
    Fvector direction;
    float contribution=0.f;
    if (source->flags.type == IRender_Light::DIRECT)
    {
        direction.invert(source->direction);
        contribution=_max(direction.dotproduct(normal),0.f)*energy;
    }
    else
    {
        const float squared=position.distance_to_sqr(source->position);
        if (squared <= source->range*source->range)
        {
            direction.sub(source->position,position).normalize_safe();
            // Keep original PLC_energy, including its unusual 1-1/(1+R).
            contribution=_max(direction.dotproduct(normal),0.f)*energy*(1.f-1.f/(1.f+_sqrt(squared)));
        }
    }
    const float cameraFade=clampr(Device.vCameraPosition.distance_to_sqr(position)/(48.f*48.f),0.f,1.f);
    const float casterFade=clampr(shadow.center.distance_to_sqr(position)/(4.5f*4.5f),0.f,1.f);
    return iCeil(255.f*(1.f-1.5f*contribution*(1.f-cameraFade)*(1.f-casterFade)));
}

void PrepareReceivers()
{
    receivers.clear();
    auto* model=g_pGameLevel->ObjectSpace.GetStaticModel();
    const auto* triangles=model->get_tris();
    const auto* vertices=model->get_verts();
    unsigned rebuilt=0,reused=0;
    for (const auto& shadow : shadows)
    {
        auto& item=cache[{shadow.object,shadow.source}];
        const auto& position=shadow.object->GetRenderData().xform.c;
        if (!item.valid || !item.objectPosition.similar(position) || !item.lightPosition.similar(shadow.source->position))
        {
            item.triangles.clear();
            Fmatrix combined=shadow.combined;
            CFrustum frustum; frustum.CreateFromMatrix(combined,FRUSTUM_P_ALL);
            collision.frustum_query(0,model,frustum);
            for (size_t index=0;index<collision.r_count();++index)
            {
                const auto* hit=collision.r_begin()+index;
                const auto& triangle=triangles[hit->id];
                if (triangle.suppress_shadows) continue;
                sPoly a,b;
                for (unsigned i=0;i<3;++i) a.push_back(vertices[triangle.verts[i]]);
                Fvector v1,v2,normal;
                v1.sub(a[0],a[1]);v2.sub(a[0],a[2]);normal.crossproduct(v1,v2);
                const float magnitude=normal.square_magnitude();
                if (magnitude < EPS_S) continue;
                normal.mul(1.f/_sqrt(magnitude));
                Fplane plane;plane.build_unit_normal(a[0],normal);
                if (plane.classify(shadow.source->position) < 0.f) continue;
                auto* clipped=frustum.ClipPoly(a,b);
                if (!clipped) continue;
                for (unsigned i=2;i<clipped->size();++i)
                    item.triangles.push_back({{(*clipped)[0],(*clipped)[i-1],(*clipped)[i]},normal});
            }
            item.objectPosition=position; item.lightPosition=shadow.source->position; item.valid=true;
            ++rebuilt;
        }
        else ++reused;
        item.time=Device.dwTimeGlobal;
        const float scale=.5f*float(tile)/size;
        const float u=float(shadow.slot%line)/line+.5f/size;
        const float v=float(shadow.slot/line)/line+.5f/size;
        for (const auto& triangle : item.triangles)
        {
            Fplane plane;plane.build_unit_normal(triangle.p[0],triangle.normal);
            if (plane.classify(Device.vCameraPosition) < 0.f) continue;
            int colors[3];
            for (unsigned i=0;i<3;++i) colors[i]=Intensity(triangle.p[i],triangle.normal,shadow);
            if (colors[0]>248 && colors[1]>248 && colors[2]>248) continue;
            for (unsigned i=0;i<3;++i)
            {
                const auto c=clampr(colors[i],32,255);
                Fvector projected;shadow.combined.transform(projected,triangle.p[i]);
                FVF::LIT vertex;
                vertex.set(triangle.p[i],color_rgba(c,c,c,c),(projected.x+1.f)*scale+u,(1.f-projected.y)*scale+v);
                receivers.push_back(vertex);
            }
        }
    }
    for (auto it=cache.begin();it!=cache.end();)
        if (Device.dwTimeGlobal-it->second.time > 30000) it=cache.erase(it);
        else ++it;
    static unsigned reports=0;
    if (reports++ < 8)
        Msg("* [olr_r1_shadow_receivers] triangles=%u rebuilt=%u reused=%u cache=%u; collision frustum clip, suppress_shadows and original PLC",unsigned(receivers.size()/3),rebuilt,reused,unsigned(cache.size()));
}
}

void BeginCollect()
{
    casters.clear(); shadows.clear(); receivers.clear();
    for (auto* fixture : fixtureSources) xr_delete(fixture);
    fixtureSources.clear();
}

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
    static bool probeUsed=false;
    const bool probe=!probeUsed && Core.Params && strstr(Core.Params,"-olr_r1_shadow_probe") &&
        strstr(Core.Params,"-olr_r1_shadow_capture");
    auto& commands=RCache;
    unsigned draws=0;
    bool bound=false;
    for (const auto& caster : casters)
    {
        auto* ros=static_cast<CROS_impl*>(caster.object->renderable_ROS());
        ros->update(caster.object);
        const float radius=caster.object->GetRenderData().visual->getVisData().sphere.R;
        auto lights=ros->lights;
        if (probe)
        {
            // Diagnostic only: a local source near this real game caster.
            // Do not modify ROS or register this light in the game database.
            auto* fixture=xr_new<light>();
            fixture->position.set(caster.center).add(Fvector().set(1.f,2.f,-1.f));
            fixture->range=12.f;
            fixtureSources.push_back(fixture);
            CROS_impl::Light selected{};selected.source=fixture;selected.energy=1.f;
            lights.push_back(selected);
        }
        for (const auto& light : lights)
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
    PrepareReceivers();
    if (probe && !receivers.empty())
    {
        // Isolated component capture on real level collision receivers with
        // a white target/empty depth. It does NOT establish full-scene depth
        // occlusion, original lamp placement, actor shadow or gameplay parity.
        auto* target=RImplementation.Target;
        target->u_setrt(commands,target->rt_Generic_1,nullptr,nullptr,target->rt_Base_Depth);
        RImplementation.rmNormal(commands);
        commands.ClearRT(target->rt_Generic_1,color_rgba(255,255,255,255));
        commands.ClearZB(target->rt_Base_Depth,1.f);
        commands.set_CullMode(CULL_CCW);
        Render();
        xr_vector<u8> pixels(Device.dwWidth*Device.dwHeight*4);
        CHK_GL(glReadPixels(0,0,Device.dwWidth,Device.dwHeight,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data()));
        unsigned nonwhite=0;
        for (size_t i=0;i<pixels.size();i+=4) nonwhite+=pixels[i]<255 || pixels[i+1]<255 || pixels[i+2]<255;
        string_path path,name;
        xr_sprintf(name,"r1-shadow-world-%llu.rgba",static_cast<unsigned long long>(CPU::QPC()));
        FS.update_path(path,"$logs$",name);
        if (auto* output=FS.w_open(path))
        {
            output->w_u32(Device.dwWidth);output->w_u32(Device.dwHeight);
            output->w(pixels.data(),pixels.size());FS.w_close(output);
        }
        Msg("* [olr_r1_shadow_fixture] synthetic=1 receiver_nonwhite_pixels=%u capture=%s; isolated empty-depth component",nonwhite,path);
        target->u_setrt(commands,Device.dwWidth,Device.dwHeight,target->get_base_rt(),0,0,target->get_base_zb());
    }
    if (probe) probeUsed=true;
    static unsigned reports=0;
    if (reports++ < 8)
        Msg("* [olr_r1_shadows] casters=%u slots=%u caster_draws=%u synthetic=%u; original projected shadow path",
            unsigned(casters.size()),unsigned(shadows.size()),draws,unsigned(!fixtureSources.empty()));
}

void Render()
{
    if (receivers.empty()) return;
    auto& commands=RCache;
    Fmatrix projection=Device.mProject;
    projection._43-=.002f; // original LightShadows receiver depth bias
    commands.set_xform_project(projection);commands.set_xform_world(Fidentity);
    commands.set_Shader(worldMaterial);commands.set_Geometry(worldGeometry);
    const unsigned before=commands.stat.render.calls;
    for (size_t first=0;first<receivers.size();)
    {
        const unsigned count=unsigned(_min(size_t(256*3),receivers.size()-first));
        u32 offset;
        auto* vertices=RImplementation.Vertex.Lock(count,worldGeometry->vb_stride,offset);
        CopyMemory(vertices,receivers.data()+first,count*sizeof(FVF::LIT));
        RImplementation.Vertex.Unlock(count,worldGeometry->vb_stride);
        commands.Render(D3DPT_TRIANGLELIST,offset,count/3);
        first+=count;
    }
    commands.set_xform_project(Device.mProject);
    static unsigned reports=0;
    if (reports++ < 8) Msg("* [olr_r1_shadow_submit] triangles=%u draws=%u multiply=DESTCOLOR_ZERO",unsigned(receivers.size()/3),commands.stat.render.calls-before);
}

void Destroy()
{
    BeginCollect(); cache.clear();
    quad.destroy();worldGeometry.destroy();blur.destroy();worldMaterial.destroy();raw.destroy();blurred.destroy();
}
}
#endif
