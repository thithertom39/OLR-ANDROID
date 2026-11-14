#pragma once

#include <luabind/object.hpp>
#include "xrCore/Events/Notifier.h"

class CALifeSmartTerrainTask;
class CSE_ALifeSmartZone;
class CSE_ALifeMonsterAbstract;
class CAI_Space;

// Holds the exact Lua userdata returned by a smart terrain while native AI
// retains its pointer. Native-only tasks keep their existing ownership rules.
class CSmartTerrainTaskOwner
{
    class ResetCallback;

    CALifeSmartTerrainTask* m_task = nullptr;
    luabind::object m_owner;
    CAI_Space* m_reset_space = nullptr;
    CEventNotifierCallback::CID m_reset_cid = CEventNotifierCallback::INVALID_CID;

    void on_script_engine_reset();

public:
    CSmartTerrainTaskOwner() = default;
    ~CSmartTerrainTaskOwner();
    CSmartTerrainTaskOwner(const CSmartTerrainTaskOwner&) = delete;
    CSmartTerrainTaskOwner& operator=(const CSmartTerrainTaskOwner&) = delete;

    CALifeSmartTerrainTask* acquire(CSE_ALifeSmartZone& zone, CSE_ALifeMonsterAbstract* monster);
    // Swaps the old root into owner, preserving it until the caller releases it.
    void assign(CALifeSmartTerrainTask* task, luabind::object& owner);
    void reset();
    CALifeSmartTerrainTask* get() const { return m_task; }
    CALifeSmartTerrainTask* operator->() const { return m_task; }
};
