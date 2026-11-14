#include "StdAfx.h"
#include "PHIsland.h"
#include "Physics.h"
#include "ph_valid_ode.h"
#include "ragdoll_audit.h"
#include "ExtendedGeom.h"
#include "IPhysicsShellHolder.h"

#include <ode/quickstep_audit.h>
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <cmath>

namespace
{
constexpr unsigned FleshBodyLimit = 64;
struct FleshBodySample
{
    dBodyID body;
    u16 object;
    pcstr section;
    dReal p[3], r[9], q[4], linear[3], angular[3], mass, inertia[9];
    Fmatrix objectTransform;
    Fvector objectPosition;
    double distance2;
    unsigned nonfinite;
};

IPhysicsShellHolder* FleshOwner(dBodyID body)
{
    if (!body || !body->geom)
        return nullptr;
    dxGeomUserData* data = retrieveGeomUserData(body->geom);
    IPhysicsShellHolder* owner = data ? data->ph_ref_object : nullptr;
    pcstr section = owner ? owner->ObjectNameSect() : nullptr;
    return section && std::strstr(section, "flesh") ? owner : nullptr;
}

bool HasFlesh(dxWorld& world)
{
    for (dxBody* b = world.firstbody; b; b = static_cast<dxBody*>(b->next))
        if (FleshOwner(b)) return true;
    return false;
}

// Fixed TLS storage: no solver-owned pointer survives its observer callback.
constexpr int SolverCaptureBodies = 16, SolverCaptureJoints = 32, SolverCaptureRows = 128;
struct SolverCaptureBody
{
    void* pointer;
    int tag, flags, object;
    dReal p[3], R[9], q[4], lv[3], av[3], f[3], t[3], mass, invMass, mc[3], I[9], invI[9];
};
struct SolverCaptureJoint
{
    void* pointer;
    int type, flags, b1, b2, offset, rows, nub, mode, num, rel[3];
    dReal cache[6], anchor1[3], anchor2[3], axis0[3], axis2[3], ref1[3], ref2[3], angle[3];
    dxJointLimitMotor limot[3];
};
struct SolverCaptureRow
{
    int b1, b2, findex;
    dReal J[12], rhs, cfm, lo, hi, lambdaPre, lambdaPost;
};
struct SolverCapturePacket
{
    bool entry, pre, post, incomplete;
    int nb, nj, m, entryNj, iterations, warm;
    u64 step, rng[3];
    dReal dt, sor, erp, cfm, gravity[3];
    SolverCaptureBody bodies[2][SolverCaptureBodies];
    SolverCaptureJoint joints[SolverCaptureJoints];
    SolverCaptureRow rows[SolverCaptureRows];
    dReal worldInvI[SolverCaptureBodies][9], cforce[SolverCaptureBodies][6];
    void Begin(u64 worldStep)
    {
        entry = pre = post = incomplete = false;
        nb = nj = m = entryNj = 0;
        step = worldStep;
    }
};
static_assert(sizeof(SolverCapturePacket) <= 65536, "Diagnostic capture must stay in bounded TLS RAM");
thread_local SolverCapturePacket solverCapturePacket;

void CaptureThree(dReal* dst, const dReal* src)
{
    for (int i=0; i!=3; ++i) dst[i]=src[i];
}
void CaptureMatrix(dReal* dst, const dReal* src)
{
    constexpr int indices[9]={0,1,2,4,5,6,8,9,10};
    for (int i=0; i!=9; ++i) dst[i]=src[indices[i]];
}
void CaptureBody(SolverCaptureBody& dst, dxBody& body)
{
    dst.pointer=&body; dst.tag=body.tag; dst.flags=body.flags;
    IPhysicsShellHolder* owner=FleshOwner(&body);
    dst.object=owner ? owner->ObjectID() : -1;
    CaptureThree(dst.p,body.posr.pos); CaptureMatrix(dst.R,body.posr.R);
    for (int i=0; i!=4; ++i) dst.q[i]=body.q[i];
    CaptureThree(dst.lv,body.lvel); CaptureThree(dst.av,body.avel);
    CaptureThree(dst.f,body.facc); CaptureThree(dst.t,body.tacc);
    dst.mass=body.mass.mass; dst.invMass=body.invMass;
    CaptureThree(dst.mc,body.mass.c); CaptureMatrix(dst.I,body.mass.I); CaptureMatrix(dst.invI,body.invI);
}
int CaptureBodyIndex(const dQuickStepAuditView& view, const dxBody* body)
{
    if (!body) return -1;
    for (int i=0; i!=view.nb; ++i) if (view.bodies[i]==body) return i;
    return -2; // Explicit graph mismatch, never silently treat it as a static body.
}
void ObserveSolverCapture(void* context, const dQuickStepAuditView* source)
{
    auto& packet=*static_cast<SolverCapturePacket*>(context);
    const auto& v=*source;
    if (v.nb<0 || v.nb>SolverCaptureBodies || v.nj<0 || v.nj>SolverCaptureJoints ||
        v.m<0 || v.m>SolverCaptureRows)
    {
        packet.incomplete=true; packet.nb=v.nb; packet.nj=v.nj; packet.m=v.m;
        return;
    }
    if (packet.incomplete) return;
    packet.nb=v.nb;
    packet.rng[unsigned(v.phase)]=v.rng_state;
    if (v.phase==dQuickStepAuditEntry)
    {
        packet.entry=true; packet.entryNj=v.nj;
        for (int i=0; i!=v.nb; ++i) CaptureBody(packet.bodies[0][i],*v.bodies[i]);
        return;
    }
    if (v.phase==dQuickStepAuditPreSOR)
    {
        packet.pre=true; packet.nj=v.nj; packet.m=v.m;
        packet.dt=v.stepsize; packet.sor=v.sor_w; packet.erp=v.erp;
        packet.cfm=v.global_cfm; packet.iterations=v.iterations; packet.warm=v.warm_starting;
        CaptureThree(packet.gravity,v.gravity);
        const auto* info=static_cast<const dxJoint::Info1*>(v.joint_info1);
        for (int i=0; i!=v.nb; ++i)
        {
            CaptureBody(packet.bodies[1][i],*v.bodies[i]);
            CaptureMatrix(packet.worldInvI[i],v.invI+12*i);
        }
        for (int i=0; i!=v.nj; ++i)
        {
            dxJoint& joint=*v.joints[i]; auto& dst=packet.joints[i];
            dst.pointer=&joint; dst.type=joint.vtable->typenum; dst.flags=joint.flags;
            dst.b1=CaptureBodyIndex(v,joint.node[0].body); dst.b2=CaptureBodyIndex(v,joint.node[1].body);
            dst.offset=v.offsets[i]; dst.rows=info[i].m; dst.nub=info[i].nub;
            if (dst.b1 < -1 || dst.b2 < -1 || dst.offset<0 || dst.rows<0 ||
                dst.offset+dst.rows>v.m || dst.rows != (i+1<v.nj ? v.offsets[i+1] : v.m)-dst.offset)
                packet.incomplete=true;
            for (int j=0; j!=6; ++j) dst.cache[j]=joint.lambda[j];
            if (dst.type==dJointTypeBall)
            {
                const auto& ball=static_cast<const dxJointBall&>(joint);
                CaptureThree(dst.anchor1,ball.anchor1); CaptureThree(dst.anchor2,ball.anchor2);
            }
            if (dst.type==dJointTypeAMotor)
            {
                const auto& motor=static_cast<const dxJointAMotor&>(joint);
                dst.mode=motor.mode; dst.num=motor.num;
                for (int j=0; j!=3; ++j) { dst.rel[j]=motor.rel[j]; dst.angle[j]=motor.angle[j]; dst.limot[j]=motor.limot[j]; }
                CaptureThree(dst.axis0,motor.axis[0]); CaptureThree(dst.axis2,motor.axis[2]);
                CaptureThree(dst.ref1,motor.reference1); CaptureThree(dst.ref2,motor.reference2);
            }
        }
        for (int i=0; i!=v.m; ++i)
        {
            auto& dst=packet.rows[i]; dst.b1=v.jb[2*i]; dst.b2=v.jb[2*i+1]; dst.findex=v.findex[i];
            for (int j=0; j!=12; ++j) dst.J[j]=v.J[12*i+j];
            dst.rhs=v.rhs[i]; dst.cfm=v.cfm[i]; dst.lo=v.lo[i]; dst.hi=v.hi[i];
            dst.lambdaPre=v.lambda ? v.lambda[i] : 0;
        }
        return;
    }
    if (v.phase==dQuickStepAuditPostSOR)
    {
        if (!packet.pre || packet.nb!=v.nb || packet.nj!=v.nj || packet.m!=v.m) { packet.incomplete=true; return; }
        packet.post=true;
        for (int i=0; i!=v.m; ++i) packet.rows[i].lambdaPost=v.lambda[i];
        for (int i=0; i!=v.nb; ++i) for (int j=0; j!=6; ++j) packet.cforce[i][j]=v.cforce[6*i+j];
    }
}
struct SolverCaptureScope
{
    dQuickStepAuditObserver observer;
    const dQuickStepAuditObserver* previous;
    bool active;
    explicit SolverCaptureScope(SolverCapturePacket* packet):observer{packet,ObserveSolverCapture},previous(nullptr),active(packet!=nullptr)
    { if (active) previous=dSetQuickStepAuditObserver(&observer); }
    ~SolverCaptureScope() { if (active) dSetQuickStepAuditObserver(previous); }
    SolverCaptureScope(const SolverCaptureScope&)=delete;
    SolverCaptureScope& operator=(const SolverCaptureScope&)=delete;
};

struct SolverCaptureWriter
{
    unsigned sequence=0, bytes=0;
    u64 hash=14695981039346656037ull;
    bool failed=false;
    void Line(const char* format, ...)
    {
        if (failed) return;
        char line[2048];
        int prefix=std::snprintf(line,sizeof(line),"[ragdoll_packet] seq=%u ",sequence);
        va_list args; va_start(args,format);
        int count=std::vsnprintf(line+prefix,sizeof(line)-unsigned(prefix),format,args); va_end(args);
        if (count<0 || unsigned(prefix+count)>=sizeof(line) || bytes+unsigned(prefix+count)+1>80*1024-256)
        { failed=true; return; }
        unsigned length=unsigned(prefix+count);
        for (unsigned i=0; i!=length; ++i) { hash^=static_cast<unsigned char>(line[i]); hash*=1099511628211ull; }
        hash^='\n'; hash*=1099511628211ull;
        bytes+=length+1; ++sequence;
        Msg("%s",line);
    }
};
void DumpSolverCapture(const SolverCapturePacket& p)
{
    SolverCaptureWriter w;
    const bool complete=p.entry && p.pre && p.post && !p.incomplete;
    // An incomplete/oversize view is never represented as a replayable packet.
    if (!complete)
        w.Line("kind=begin version=1 step=%llu nb=%d nj=%d m=%d incomplete=1",static_cast<unsigned long long>(p.step),p.nb,p.nj,p.m);
    else
    {
        w.Line("kind=begin version=1 step=%llu nb=%d nj=%d m=%d entry_nj=%d dt=%.9g sor=%.9g erp=%.9g cfm=%.9g iterations=%d warm=%d gravity=(%.9g,%.9g,%.9g) rng_entry=%llu rng_pre=%llu rng_post=%llu",
            static_cast<unsigned long long>(p.step),p.nb,p.nj,p.m,p.entryNj,p.dt,p.sor,p.erp,p.cfm,p.iterations,p.warm,p.gravity[0],p.gravity[1],p.gravity[2],
            static_cast<unsigned long long>(p.rng[0]),static_cast<unsigned long long>(p.rng[1]),static_cast<unsigned long long>(p.rng[2]));
        for (int phase=0; phase!=2; ++phase) for (int i=0; i!=p.nb; ++i)
        {
            const auto& b=p.bodies[phase][i];
            w.Line("kind=body phase=%s i=%d ptr=%p object=%d tag=%d flags=%d p=(%.9g,%.9g,%.9g) R=(%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g) q=(%.9g,%.9g,%.9g,%.9g) lv=(%.9g,%.9g,%.9g) av=(%.9g,%.9g,%.9g) f=(%.9g,%.9g,%.9g) t=(%.9g,%.9g,%.9g) mass=%.9g invMass=%.9g mc=(%.9g,%.9g,%.9g) I=(%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g) invI=(%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g)",
                phase ? "pre" : "entry",i,b.pointer,b.object,b.tag,b.flags,b.p[0],b.p[1],b.p[2],
                b.R[0],b.R[1],b.R[2],b.R[3],b.R[4],b.R[5],b.R[6],b.R[7],b.R[8],b.q[0],b.q[1],b.q[2],b.q[3],
                b.lv[0],b.lv[1],b.lv[2],b.av[0],b.av[1],b.av[2],b.f[0],b.f[1],b.f[2],b.t[0],b.t[1],b.t[2],b.mass,b.invMass,b.mc[0],b.mc[1],b.mc[2],
                b.I[0],b.I[1],b.I[2],b.I[3],b.I[4],b.I[5],b.I[6],b.I[7],b.I[8],b.invI[0],b.invI[1],b.invI[2],b.invI[3],b.invI[4],b.invI[5],b.invI[6],b.invI[7],b.invI[8]);
        }
        for (int i=0; i!=p.nb; ++i)
        {
            const auto* v=p.worldInvI[i];
            w.Line("kind=wi i=%d values=(%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g)",i,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8]);
        }
        for (int i=0; i!=p.nj; ++i)
        {
            const auto& j=p.joints[i];
            w.Line("kind=joint i=%d ptr=%p type=%d flags=%d b1=%d b2=%d offset=%d rows=%d nub=%d cache=(%.9g,%.9g,%.9g,%.9g,%.9g,%.9g)",i,j.pointer,j.type,j.flags,j.b1,j.b2,j.offset,j.rows,j.nub,j.cache[0],j.cache[1],j.cache[2],j.cache[3],j.cache[4],j.cache[5]);
            if (j.type==dJointTypeBall)
                w.Line("kind=ball i=%d anchor1=(%.9g,%.9g,%.9g) anchor2=(%.9g,%.9g,%.9g)",i,j.anchor1[0],j.anchor1[1],j.anchor1[2],j.anchor2[0],j.anchor2[1],j.anchor2[2]);
            if (j.type==dJointTypeAMotor)
            {
                w.Line("kind=amotor i=%d mode=%d num=%d rel=(%d,%d,%d) axis0=(%.9g,%.9g,%.9g) axis2=(%.9g,%.9g,%.9g) ref1=(%.9g,%.9g,%.9g) ref2=(%.9g,%.9g,%.9g) angle=(%.9g,%.9g,%.9g)",i,j.mode,j.num,j.rel[0],j.rel[1],j.rel[2],j.axis0[0],j.axis0[1],j.axis0[2],j.axis2[0],j.axis2[1],j.axis2[2],j.ref1[0],j.ref1[1],j.ref1[2],j.ref2[0],j.ref2[1],j.ref2[2],j.angle[0],j.angle[1],j.angle[2]);
                for (int a=0; a!=3; ++a)
                {
                    const auto& l=j.limot[a];
                    w.Line("kind=limot joint=%d axis=%d lostop=%.9g histop=%.9g limit=%d limit_err=%.9g fmax=%.9g vel=%.9g normal_cfm=%.9g stop_cfm=%.9g stop_erp=%.9g fudge=%.9g bounce=%.9g",i,a,l.lostop,l.histop,l.limit,l.limit_err,l.fmax,l.vel,l.normal_cfm,l.stop_cfm,l.stop_erp,l.fudge_factor,l.bounce);
                }
            }
        }
        for (int i=0; i!=p.m; ++i)
        {
            const auto& r=p.rows[i];
            w.Line("kind=row i=%d b1=%d b2=%d findex=%d J=(%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g) rhs=%.9g cfm=%.9g lo=%.9g hi=%.9g lambda_pre=%.9g lambda_post=%.9g",i,r.b1,r.b2,r.findex,r.J[0],r.J[1],r.J[2],r.J[3],r.J[4],r.J[5],r.J[6],r.J[7],r.J[8],r.J[9],r.J[10],r.J[11],r.rhs,r.cfm,r.lo,r.hi,r.lambdaPre,r.lambdaPost);
        }
        for (int i=0; i!=p.nb; ++i)
        {
            const auto* v=p.cforce[i];
            w.Line("kind=fc i=%d values=(%.9g,%.9g,%.9g,%.9g,%.9g,%.9g)",i,v[0],v[1],v[2],v[3],v[4],v[5]);
        }
    }
    Msg("[ragdoll_packet] seq=%u kind=end complete=%u reason=%s lines=%u hash=%016llx",w.sequence,unsigned(complete && !w.failed),
        w.failed ? "text_limit" : complete ? "ok" : "view_limit_or_missing_phase",w.sequence,static_cast<unsigned long long>(w.hash));
}

FleshBodySample ReadFleshBody(dBodyID body, IPhysicsShellHolder& owner)
{
    FleshBodySample s{};
    s.body = body;
    s.object = owner.ObjectID();
    s.section = owner.ObjectNameSect();
    s.objectTransform = owner.ObjectXFORM();
    s.objectPosition = owner.ObjectPosition();
    s.nonfinite = 0;
    s.distance2 = 0;
    const dReal* pos = dBodyGetPosition(body);
    const dReal* rot = dBodyGetRotation(body);
    const dReal* quat = dBodyGetQuaternion(body);
    const dReal* linear = dBodyGetLinearVel(body);
    const dReal* angular = dBodyGetAngularVel(body);
    dMass mass;
    dBodyGetMass(body, &mass);
    s.mass = mass.mass;
    s.nonfinite += !std::isfinite(s.mass);
    constexpr unsigned ri[9] = {0, 1, 2, 4, 5, 6, 8, 9, 10};
    for (unsigned i = 0; i != 3; ++i)
    {
        s.p[i] = pos[i];
        s.linear[i] = linear[i];
        s.angular[i] = angular[i];
        s.nonfinite += !std::isfinite(s.p[i]) || !std::isfinite(s.linear[i]) || !std::isfinite(s.angular[i]);
    }
    for (unsigned i = 0; i != 9; ++i)
    {
        s.r[i] = rot[ri[i]];
        s.inertia[i] = mass.I[ri[i]];
        s.nonfinite += !std::isfinite(s.r[i]) || !std::isfinite(s.inertia[i]);
    }
    for (unsigned i = 0; i != 4; ++i)
    {
        s.q[i] = quat[i];
        s.nonfinite += !std::isfinite(s.q[i]);
    }
    const Fvector* axes[] = {&s.objectTransform.i, &s.objectTransform.j, &s.objectTransform.k,
        &s.objectTransform.c, &s.objectPosition};
    for (const Fvector* v : axes)
        s.nonfinite += !std::isfinite(v->x) || !std::isfinite(v->y) || !std::isfinite(v->z);
    const double dx = double(s.p[0]) - s.objectPosition.x;
    const double dy = double(s.p[1]) - s.objectPosition.y;
    const double dz = double(s.p[2]) - s.objectPosition.z;
    s.distance2 = dx * dx + dy * dy + dz * dz;
    return s;
}

struct FleshSnapshot
{
    FleshBodySample samples[FleshBodyLimit];
    FleshBodySample worst;
    unsigned count, total, enabled, contacts, nonfinite;
    double maxDistance2, maxAnchor2, maxDepth, maxNormalError;
};

FleshSnapshot ObserveFlesh(dxWorld& world)
{
    FleshSnapshot s{};
    s.count = s.total = s.enabled = s.contacts = s.nonfinite = 0;
    s.maxDistance2 = s.maxAnchor2 = s.maxDepth = s.maxNormalError = 0;
    for (dxBody* body = world.firstbody; body; body = static_cast<dxBody*>(body->next))
    {
        IPhysicsShellHolder* owner = FleshOwner(body);
        if (!owner) continue;
        const FleshBodySample sample = ReadFleshBody(body, *owner);
        if (s.total == 0 || (!s.worst.nonfinite &&
                (sample.nonfinite || sample.distance2 > s.worst.distance2)))
            s.worst = sample;
        if (sample.distance2 > s.maxDistance2) s.maxDistance2 = sample.distance2;
        if (s.count < FleshBodyLimit) s.samples[s.count++] = sample;
        ++s.total;
        s.enabled += dBodyIsEnabled(body) != 0;
        s.nonfinite += sample.nonfinite;
    }
    for (dxJoint* joint = world.firstjoint; joint; joint = static_cast<dxJoint*>(joint->next))
    {
        const int type = dJointGetType(joint);
        if (type == dJointTypeContact) ++s.contacts;
        if (!FleshOwner(dJointGetBody(joint, 0)) && !FleshOwner(dJointGetBody(joint, 1))) continue;
        if (type == dJointTypeContact)
        {
            const dContactGeom& g = static_cast<dxJointContact*>(joint)->contact.geom;
            double normal2 = 0;
            s.nonfinite += !std::isfinite(g.depth);
            for (unsigned i = 0; i != 3; ++i)
            {
                s.nonfinite += !std::isfinite(g.normal[i]);
                normal2 += double(g.normal[i]) * g.normal[i];
            }
            if (g.depth > s.maxDepth) s.maxDepth = g.depth;
            const double error = std::fabs(normal2 - 1.0);
            if (error > s.maxNormalError) s.maxNormalError = error;
            continue;
        }
        dVector3 a, b;
        if (type == dJointTypeBall)
        {
            dJointGetBallAnchor(joint, a); dJointGetBallAnchor2(joint, b);
        }
        else if (type == dJointTypeHinge)
        {
            dJointGetHingeAnchor(joint, a); dJointGetHingeAnchor2(joint, b);
        }
        else continue;
        double error2 = 0;
        for (unsigned i = 0; i != 3; ++i)
        {
            s.nonfinite += !std::isfinite(a[i]) || !std::isfinite(b[i]);
            const double error = double(a[i]) - b[i]; error2 += error * error;
        }
        if (error2 > s.maxAnchor2) s.maxAnchor2 = error2;
    }
    return s;
}

void LogFleshBody(const char* phase, const FleshBodySample& s)
{
    const Fmatrix& m = s.objectTransform;
    Msg("[ragdoll_trace] phase=%s body=%p object=%u section=%s p=(%g,%g,%g) "
        "R=(%g,%g,%g;%g,%g,%g;%g,%g,%g) q=(%g,%g,%g,%g) linear=(%g,%g,%g) angular=(%g,%g,%g)",
        phase, static_cast<void*>(s.body), unsigned(s.object), s.section, s.p[0], s.p[1], s.p[2],
        s.r[0], s.r[1], s.r[2], s.r[3], s.r[4], s.r[5], s.r[6], s.r[7], s.r[8],
        s.q[0], s.q[1], s.q[2], s.q[3], s.linear[0], s.linear[1], s.linear[2],
        s.angular[0], s.angular[1], s.angular[2]);
    Msg("[ragdoll_trace] phase=%s body=%p owner=(%g,%g,%g;%g,%g,%g;%g,%g,%g;%g,%g,%g) "
        "owner_position=(%g,%g,%g) distance=%g bad=%u",
        phase, static_cast<void*>(s.body), m.i.x, m.i.y, m.i.z, m.j.x, m.j.y, m.j.z,
        m.k.x, m.k.y, m.k.z, m.c.x, m.c.y, m.c.z,
        s.objectPosition.x, s.objectPosition.y, s.objectPosition.z, std::sqrt(s.distance2), s.nonfinite);
    Msg("[ragdoll_trace] phase=%s body=%p mass=%g inertia=(%g,%g,%g;%g,%g,%g;%g,%g,%g)",
        phase, static_cast<void*>(s.body), s.mass, s.inertia[0], s.inertia[1], s.inertia[2],
        s.inertia[3], s.inertia[4], s.inertia[5], s.inertia[6], s.inertia[7], s.inertia[8]);
}

struct FleshAuditBudget { u64 lastStep = 0; unsigned ready = 0, anomalies = 0; bool packetCaptured = false; } fleshBudget;
}

void CPHIsland::Step(dReal step)
{
    if (!m_flags.is_active()) return;
    const bool exact = m_flags.is_exact_integration_prefeared() && nj < max_joint_allowed_for_exeact_integration;
    const bool audit = ph_world && ragdoll_audit::Enabled();
    if (audit)
    {
        if (ph_world->StepsNum() < fleshBudget.lastStep) fleshBudget = {};
        fleshBudget.lastStep = ph_world->StepsNum();
    }
    // Only the selected mutation chain is observed. Other object types cannot
    // consume the anomaly reserve; there is no timeout before the flesh dies.
    if (!audit || fleshBudget.anomalies >= 8 || !HasFlesh(*DWorld()))
    {
        if (exact) dWorldStep(DWorld(), fixed_step);
        else dWorldQuickStep(DWorld(), fixed_step);
        return;
    }
    const FleshSnapshot before = ObserveFlesh(*DWorld());
    if (!before.total)
    {
        if (exact) dWorldStep(DWorld(), fixed_step);
        else dWorldQuickStep(DWorld(), fixed_step);
        return;
    }
    const bool fullCapture = !exact && !fleshBudget.packetCaptured;
    if (fullCapture) solverCapturePacket.Begin(ph_world->StepsNum());
    std::chrono::steady_clock::time_point start, end;
    {
        SolverCaptureScope scope(fullCapture ? &solverCapturePacket : nullptr);
        start = std::chrono::steady_clock::now();
        if (exact) dWorldStep(DWorld(), fixed_step);
        else dWorldQuickStep(DWorld(), fixed_step);
        end = std::chrono::steady_clock::now();
    }
    const FleshSnapshot after = ObserveFlesh(*DWorld());
    const bool anomaly = before.nonfinite || after.nonfinite || before.maxDistance2 > 64.0 ||
        after.maxDistance2 > 64.0 || before.maxAnchor2 > 64.0 || after.maxAnchor2 > 64.0;
    if (anomaly && fullCapture)
    {
        fleshBudget.packetCaptured = true;
        DumpSolverCapture(solverCapturePacket);
    }
    if (!anomaly && fleshBudget.ready >= 4) return;
    if (anomaly) ++fleshBudget.anomalies;
    else ++fleshBudget.ready;
    // The after-worst body may differ from the before-worst body. Find its
    // copied pre-step state instead of presenting mismatched bodies as a pair.
    const FleshBodySample& chosen = after.total ? after.worst : before.worst;
    const FleshBodySample* matched = nullptr;
    for (unsigned i = 0; i != before.count; ++i)
        if (before.samples[i].body == chosen.body) { matched = &before.samples[i]; break; }
    Msg("[ragdoll_trace] phase=solver island=%p step=%llu solver=%s anomaly=%u record=%u/8 ready=%u/4 "
        "bodies=%d joints=%d island_contacts=%u flesh_bodies=%u flesh_enabled=%u sampled_before=%u "
        "matched=%u bad_before=%u bad_after=%u distance_before=%g distance_after=%g "
        "anchor_before=%g anchor_after=%g contact_depth_before=%g contact_depth_after=%g "
        "normal_error_before=%g normal_error_after=%g solver_wall_ms=%.3f",
        static_cast<void*>(this), static_cast<unsigned long long>(ph_world->StepsNum()), exact ? "exact" : "quick",
        unsigned(anomaly), fleshBudget.anomalies, fleshBudget.ready, nb, nj, after.contacts, after.total,
        after.enabled, before.count, unsigned(matched != nullptr), before.nonfinite, after.nonfinite,
        std::sqrt(before.maxDistance2), std::sqrt(after.maxDistance2), std::sqrt(before.maxAnchor2),
        std::sqrt(after.maxAnchor2), before.maxDepth, after.maxDepth, before.maxNormalError,
        after.maxNormalError, std::chrono::duration<double, std::milli>(end - start).count());
    if (matched) LogFleshBody("ode_before", *matched);
    else LogFleshBody("ode_before_unmatched_worst", before.worst);
    if (after.total) LogFleshBody("ode_after", chosen);
    else Msg("[ragdoll_trace] phase=ode_after target_absent=1");
}

void CPHIsland::Enable()
{
    if (!m_flags.is_active())
        return;

    for (dxBody* body = DWorld()->firstbody; body; body = (dxBody*)body->next)
        body->flags &= ~dxBodyDisabled;
}
void CPHIsland::Repair()
{
    if (!m_flags.is_active())
        return;
    dBodyID body;
    for (body = firstbody; body; body = (dxBody*)body->next)
    {
        if (!dV_valid(dBodyGetAngularVel(body)))
            dBodySetAngularVel(body, 0.f, 0.f, 0.f);
        if (!dV_valid(dBodyGetLinearVel(body)))
            dBodySetLinearVel(body, 0.f, 0.f, 0.f);
        if (!dV_valid(dBodyGetPosition(body)))
            dBodySetPosition(body, 0.f, 0.f, 0.f);
        if (!dQ_valid(dBodyGetQuaternion(body)))
        {
            dQuaternion q = {1.f, 0.f, 0.f, 0.f}; // dQSetIdentity(q);
            dBodySetQuaternion(body, q);
        }
    }
}
