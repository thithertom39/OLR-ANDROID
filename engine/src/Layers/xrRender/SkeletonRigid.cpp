//---------------------------------------------------------------------------
#include "stdafx.h"
#pragma hdrstop

#include "SkeletonCustom.h"

#if defined(XR_PLATFORM_ANDROID)
#include <cmath>
#include <cstring>
#endif

namespace xray::render::RENDER_NAMESPACE
{
extern int psSkeletonUpdate;

#if defined(XR_PLATFORM_ANDROID)
namespace RagdollAudit
{
constexpr u32 MaxRecords = 8;
constexpr u32 MaxBoneLines = 16;
// Conservative suspicious model-space extent for this opt-in audit; legitimate
// oversized skeletons can also trigger it. The existing DEBUG 1000 check stays.
constexpr float MaxRadius = 64.f;
u32 records = 0; // Accessed under CalculateBones' UCalc_Mutex.

bool Enabled()
{
    static const bool enabled = []
    {
        if (!Core.Params)
            return false;
        constexpr char flag[] = "-ragdoll_audit";
        constexpr size_t length = sizeof(flag) - 1;
        for (pcstr arg = Core.Params; (arg = std::strstr(arg, flag)) != nullptr; ++arg)
        {
            if ((arg == Core.Params || std::strchr(" \t\r\n", arg[-1])) &&
                (arg[length] == '\0' || std::strchr(" \t\r\n", arg[length])))
                return true;
        }
        return false;
    }();
    return enabled;
}

bool CanScan() { return Enabled() && records < MaxRecords; }

bool Finite(const Fvector& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool Finite(const Fmatrix& m)
{
    // These are the twelve components used by affine skinning/bounds math.
    return Finite(m.i) && Finite(m.j) && Finite(m.k) && Finite(m.c);
}

bool BoneAnomaly(const CBoneInstance& bone)
{
    return !Finite(bone.mTransform) || !Finite(bone.mRenderTransform) ||
        bone.mTransform.c.square_magnitude() > MaxRadius * MaxRadius ||
        bone.mRenderTransform.c.square_magnitude() > MaxRadius * MaxRadius;
}

bool BoundsAnomaly(const Fbox& box, float radius)
{
    return !Finite(box.vMin) || !Finite(box.vMax) || !box.is_valid() ||
        !std::isfinite(radius) || radius < 0.f || radius > MaxRadius;
}

bool BeginRecord(bool anomaly)
{
    if (!anomaly || !CanScan())
        return false;
    ++records;
    return true;
}
} // namespace RagdollAudit
#endif

#ifdef DEBUG
void check_kinematics(CKinematics* _k, LPCSTR s);
#endif

void CKinematics::CalculateBones(BOOL bForceExact)
{
    ZoneScoped;

    // early out.
    // check if the info is still relevant
    // skip all the computations - assume nothing changes in a small period of time :)
    if (Device.dwTimeGlobal == UCalc_Time)
        return; // early out for "fast" update
    UCalc_mtlock lock;
    OnCalculateBones();
    if (!bForceExact && (Device.dwTimeGlobal < (UCalc_Time + UCalc_Interval)))
        return; // early out for "slow" update
    if (Update_Visibility)
        Visibility_Update();

    _DBG_SINGLE_USE_MARKER;
    // here we have either:
    //	1:	timeout elapsed
    //	2:	exact computation required
    UCalc_Time = Device.dwTimeGlobal;

// exact computation
// Calculate bones
#ifdef DEBUG
    RImplementation.BasicStats.Animation.Begin();
#endif

    Bone_Calculate(bones->at(iRoot), &Fidentity);
#ifdef DEBUG
    check_kinematics(this, dbg_name.c_str());
    RImplementation.BasicStats.Animation.End();
#endif
    VERIFY(LL_GetBonesVisible() != 0);
    // Calculate BOXes/Spheres if needed
    UCalc_Visibox++;
    if (UCalc_Visibox >= psSkeletonUpdate)
    {
        ZoneScopedN("Skeleton update");

        // mark
        UCalc_Visibox = -(::Random.randI(psSkeletonUpdate - 1));

        // the update itself
        Fbox Box;
        Box.invalidate();
#if defined(XR_PLATFORM_ANDROID)
        const bool audit = RagdollAudit::CanScan();
        u32 firstBadBone = u32(-1);
        u32 farthestBone = u32(-1);
        float farthestSquared = 0.f;
#endif
        for (u32 b = 0; b < bones->size(); b++)
        {
            if (!LL_GetBoneVisible(u16(b)))
                continue;
            Fobb& obb = (*bones)[b]->obb;
            Fmatrix& Mbone = bone_instances[b].mTransform;
#if defined(XR_PLATFORM_ANDROID)
            if (audit)
            {
                if (firstBadBone == u32(-1) && RagdollAudit::BoneAnomaly(bone_instances[b]))
                    firstBadBone = b;
                const float distanceSquared = Mbone.c.square_magnitude();
                if (distanceSquared > farthestSquared)
                {
                    farthestSquared = distanceSquared;
                    farthestBone = b;
                }
            }
#endif
            Fmatrix Mbox;
            obb.xform_get(Mbox);
            Fmatrix X;
            X.mul_43(Mbone, Mbox);
            Fvector& S = obb.m_halfsize;

            Fvector P, A;
            A.set(-S.x, -S.y, -S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
            A.set(-S.x, -S.y, S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
            A.set(S.x, -S.y, S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
            A.set(S.x, -S.y, -S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
            A.set(-S.x, S.y, -S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
            A.set(-S.x, S.y, S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
            A.set(S.x, S.y, S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
            A.set(S.x, S.y, -S.z);
            X.transform_tiny(P, A);
            Box.modify(P);
        }
        if (bones->size())
        {
            // previous frame we have updated box - update sphere
            vis.box.vMin = (Box.vMin);
            vis.box.vMax = (Box.vMax);
            vis.box.getsphere(vis.sphere.P, vis.sphere.R);
        }
#if defined(XR_PLATFORM_ANDROID)
        if (audit && RagdollAudit::BeginRecord(firstBadBone != u32(-1) ||
                RagdollAudit::BoundsAnomaly(vis.box, vis.sphere.R)))
        {
#ifdef DEBUG
            pcstr modelName = dbg_name.c_str();
#else
            // Model file names are not retained by dxRender_Visual in Release.
            pcstr modelName = "<unavailable-release>";
#endif
            pcstr rootName = LL_BoneName_dbg(iRoot);
            Msg("! [ragdoll_audit] record=%u/%u model=%s skeleton=%p type=%u root=%s bones=%zu "
                "radius=%g bbox=(%g,%g,%g)-(%g,%g,%g) first_bad=%u max_translation_sq=%g",
                RagdollAudit::records, RagdollAudit::MaxRecords, modelName ? modelName : "<unnamed>",
                static_cast<void*>(this), Type, rootName ? rootName : "<unnamed>", bones->size(),
                vis.sphere.R, vis.box.vMin.x, vis.box.vMin.y, vis.box.vMin.z,
                vis.box.vMax.x, vis.box.vMax.y, vis.box.vMax.z, firstBadBone, farthestSquared);
            u32 lines = 0;
            const auto logBone = [&](u32 b)
            {
                CBoneInstance& bone = bone_instances[b];
                pcstr boneName = LL_BoneName_dbg(u16(b));
                Msg("! [ragdoll_audit] bone=%u name=%s visible=%u cpu_finite=%u render_finite=%u "
                    "cpu=(%g,%g,%g) render=(%g,%g,%g) callback_type=%u callback=%u overwrite=%d",
                    b, boneName ? boneName : "<unnamed>", !!LL_GetBoneVisible(u16(b)),
                    RagdollAudit::Finite(bone.mTransform), RagdollAudit::Finite(bone.mRenderTransform),
                    bone.mTransform.c.x, bone.mTransform.c.y, bone.mTransform.c.z,
                    bone.mRenderTransform.c.x, bone.mRenderTransform.c.y, bone.mRenderTransform.c.z,
                    bone.callback_type(), bone.callback() != nullptr, bone.callback_overwrite());
                ++lines;
            };
            // Keep the suspect/extreme bones even when the skeleton has more
            // bones than the log budget; only then include the remaining pose.
            if (firstBadBone != u32(-1))
                logBone(firstBadBone);
            if (farthestBone != u32(-1) && farthestBone != firstBadBone)
                logBone(farthestBone);
            for (u32 b = 0; b < bones->size() && lines < RagdollAudit::MaxBoneLines; ++b)
            {
                if (b != firstBadBone && b != farthestBone)
                    logBone(b);
            }
        }
#endif
#ifdef DEBUG
        // Validate
        VERIFY3(_valid(vis.box.vMin) && _valid(vis.box.vMax), "Invalid bones-xform in model", dbg_name.c_str());
        if (vis.sphere.R > 1000.f)
        {
            for (u16 ii = 0; ii < LL_BoneCount(); ++ii)
            {
                Fmatrix tr;
                tr = LL_GetTransform(ii);
                Log("bone ", LL_BoneName_dbg(ii));
                Log("bone_matrix", tr);
            }
            Log("end-------");
        }
        VERIFY3(vis.sphere.R < 1000.f, "Invalid bones-xform in model", dbg_name.c_str());
#endif
    }

    //
    if (Update_Callback)
        Update_Callback(this);
}

#ifdef DEBUG
void check_kinematics(CKinematics* _k, LPCSTR s)
{
    CKinematics* K = _k;
    Fmatrix& MrootBone = K->LL_GetBoneInstance(K->LL_GetBoneRoot()).mTransform;
    if (MrootBone.c.y > 10000)
    {
        Msg("all bones transform:--------[%s]", s);

        for (u16 ii = 0; ii < K->LL_BoneCount(); ++ii)
        {
            Fmatrix tr;

            tr = K->LL_GetTransform(ii);
            Log("bone ", K->LL_BoneName_dbg(ii));
            Log("bone_matrix", tr);
        }
        Log("end-------");
        VERIFY3(0, "check_kinematics failed for ", s);
    }
}
#endif

// Добавить скриптовое смещение для кости --#SM+#--
void CKinematics::LL_AddTransformToBone(KinematicsABT::additional_bone_transform& offset)
{
    m_bones_offsets.push_back(offset);
}

// Обнулить скриптовое смещение для конкретной кости или всех сразу (bone_id = BI_NONE) --#SM+#--
void CKinematics::LL_ClearAdditionalTransform(u16 bone_id)
{
    if (bone_id == BI_NONE)
    {
        m_bones_offsets.clear();
        return;
    }

    BONE_TRANSFORM_VECTOR_IT it = m_bones_offsets.begin();
    while (it != m_bones_offsets.end())
    {
        if (it->m_bone_id == bone_id)
        {
            it = m_bones_offsets.erase(it);
        }
        else
            ++it;
    }
}

void CKinematics::BuildBoneMatrix(
    const CBoneData* bd, CBoneInstance& bi, const Fmatrix* parent, u8 channel_mask /*= (1<<0)*/)
{
    bi.mTransform.mul_43(*parent, bd->bind_transform);
    CalculateBonesAdditionalTransforms(bd, bi, parent, channel_mask); //--#SM+#--
}

// Добавляем константные смещения к нужным костям --#SM+#--
void CKinematics::CalculateBonesAdditionalTransforms(
    const CBoneData* bd, CBoneInstance& bi, const Fmatrix* parent, u8 channel_mask /* = (1<<0)*/)
{
    // bi.mTransform.c - содержит смещение относительно первой кости модели\центра сцены (0, 0, 0)
    for (auto& it : m_bones_offsets)
    {
        if (it.m_bone_id == bd->GetSelfID())
        {
            const Fvector vOldPos = bi.mTransform.c;
            bi.mTransform.mulB_43(it.m_transform); // Rotation
            bi.mTransform.c.add(vOldPos, it.m_transform.c); // Translation
        }
    }
}

void CKinematics::CLBone(const CBoneData* bd, CBoneInstance& bi, const Fmatrix* parent, u8 channel_mask /*= (1<<0)*/)
{
    ZoneScoped;

    u16 SelfID = bd->GetSelfID();

    if (LL_GetBoneVisible(SelfID))
    {
        if (bi.callback_overwrite())
        {
            if (bi.callback())
                bi.callback()(&bi);
        }
        else
        {
            BuildBoneMatrix(bd, bi, parent, channel_mask);
#ifndef MASTER_GOLD
            R_ASSERT2(_valid(bi.mTransform), "anim kils bone matrix");
#endif // #ifndef MASTER_GOLD
            if (bi.callback())
            {
                bi.callback()(&bi);
#ifndef MASTER_GOLD
                R_ASSERT2(_valid(bi.mTransform), make_string("callback kils bone matrix bone: %s ", bd->name.c_str()));
#endif // #ifndef MASTER_GOLD
            }
        }
        bi.mRenderTransform.mul_43(bi.mTransform, bd->m2b_transform);
    }
}

void CKinematics::Bone_GetAnimPos(Fmatrix& pos, u16 id, u8 mask_channel, bool ignore_callbacks)
{
    ZoneScoped;

    R_ASSERT(id < LL_BoneCount());
    CBoneInstance bi = LL_GetBoneInstance(id);
    BoneChain_Calculate(&LL_GetData(id), bi, mask_channel, ignore_callbacks);
#ifndef MASTER_GOLD
    R_ASSERT(_valid(bi.mTransform));
#endif
    pos.set(bi.mTransform);
}

void CKinematics::Bone_Calculate(CBoneData* bd, Fmatrix* parent)
{
    ZoneScoped;

    u16 SelfID = bd->GetSelfID();
    CBoneInstance& BONE_INST = LL_GetBoneInstance(SelfID);
    CLBone(bd, BONE_INST, parent, u8(-1));
    // Calculate children
    for (xr_vector<CBoneData*>::iterator C = bd->children.begin(); C != bd->children.end(); ++C)
        Bone_Calculate(*C, &BONE_INST.mTransform);
}

void CKinematics::BoneChain_Calculate(const CBoneData* bd, CBoneInstance& bi, u8 mask_channel, bool ignore_callbacks)
{
    ZoneScoped;

    u16 SelfID = bd->GetSelfID();
    // CBlendInstance& BLEND_INST	= LL_GetBlendInstance(SelfID);
    // CBlendInstance::BlendSVec &Blend = BLEND_INST.blend_vector();
    // ignore callbacks
    BoneCallback bc = bi.callback();
    BOOL ow = bi.callback_overwrite();
    if (ignore_callbacks)
    {
        bi.set_callback(bi.callback_type(), nullptr, bi.callback_param(), 0);
    }
    if (SelfID == LL_GetBoneRoot())
    {
        CLBone(bd, bi, &Fidentity, mask_channel);
        // restore callback
        bi.set_callback(bi.callback_type(), bc, bi.callback_param(), ow);
        return;
    }
    u16 ParentID = bd->GetParentID();
    R_ASSERT(ParentID != BI_NONE);
    CBoneData* ParrentDT = &LL_GetData(ParentID);
    CBoneInstance parrent_bi = LL_GetBoneInstance(ParentID);
    BoneChain_Calculate(ParrentDT, parrent_bi, mask_channel, ignore_callbacks);
    CLBone(bd, bi, &parrent_bi.mTransform, mask_channel);
    // restore callback
    bi.set_callback(bi.callback_type(), bc, bi.callback_param(), ow);
}
} // namespace xray::render::RENDER_NAMESPACE
