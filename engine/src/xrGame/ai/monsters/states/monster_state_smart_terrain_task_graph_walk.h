#pragma once

#include "ai/monsters/states/monster_state_move.h"
#include "alife_smart_terrain_task_owner.h"

template <typename _Object>
class CStateMonsterSmartTerrainTaskGraphWalk : public CMonsterStateMove<_Object>
{
    typedef CMonsterStateMove<_Object> inherited;

    CSmartTerrainTaskOwner m_task;

public:
    CStateMonsterSmartTerrainTaskGraphWalk(_Object* obj) : inherited(obj) {}
    virtual ~CStateMonsterSmartTerrainTaskGraphWalk() { release_task(); }
    void release_task() { m_task.reset(); }
    virtual void initialize();
    virtual void reinit();
    virtual void finalize();
    virtual void critical_finalize();
    virtual void execute();
    virtual bool check_start_conditions();
    virtual bool check_completion();
    virtual void remove_links(IGameObject* object) { inherited::remove_links(object); }
};

#include "monster_state_smart_terrain_task_graph_walk_inline.h"
