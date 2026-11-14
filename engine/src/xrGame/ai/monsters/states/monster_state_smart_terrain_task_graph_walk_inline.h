#pragma once

#define TEMPLATE_SPECIALIZATION \
    template <typename _Object\
>

#define CStateMonsterSmartTerrainTaskGraphWalkAbstract CStateMonsterSmartTerrainTaskGraphWalk<_Object>

TEMPLATE_SPECIALIZATION
void CStateMonsterSmartTerrainTaskGraphWalkAbstract::reinit()
{
    inherited::reinit();
    release_task();
}

TEMPLATE_SPECIALIZATION
void CStateMonsterSmartTerrainTaskGraphWalkAbstract::finalize()
{
    inherited::finalize();
    release_task();
}

TEMPLATE_SPECIALIZATION
void CStateMonsterSmartTerrainTaskGraphWalkAbstract::critical_finalize()
{
    inherited::critical_finalize();
    release_task();
}

TEMPLATE_SPECIALIZATION
void CStateMonsterSmartTerrainTaskGraphWalkAbstract::initialize()
{
    inherited::initialize();

    CSE_ALifeMonsterAbstract* monster =
        smart_cast<CSE_ALifeMonsterAbstract*>(ai().alife().objects().object(this->object->ID()));
    VERIFY(monster);
    VERIFY(monster->m_smart_terrain_id != 0xffff);

    // get task
    m_task.acquire(monster->brain().smart_terrain(), monster);
    if (CALifeSmartTerrainTask::audit_enabled())
        CALifeSmartTerrainTask::audit_use(m_task.get(), this, this->object->ID(), this->object->cName().c_str(),
            "graph_initialize", true, monster->m_smart_terrain_id);
    VERIFY(m_task.get());
}

TEMPLATE_SPECIALIZATION
bool CStateMonsterSmartTerrainTaskGraphWalkAbstract::check_start_conditions()
{
    CSE_ALifeMonsterAbstract* monster =
        smart_cast<CSE_ALifeMonsterAbstract*>(ai().alife().objects().object(this->object->ID()));
    VERIFY(monster);

    if (monster->m_smart_terrain_id == 0xffff)
    {
        release_task();
        return false;
    }

    m_task.acquire(monster->brain().smart_terrain(), monster);
    if (CALifeSmartTerrainTask::audit_enabled())
        CALifeSmartTerrainTask::audit_use(m_task.get(), this, this->object->ID(), this->object->cName().c_str(),
            "graph_start", true, monster->m_smart_terrain_id);
    VERIFY3(m_task.get(), "Smart terrain selected, but task was not set for monster ", this->object->cName().c_str());
    if (this->object->ai_location().game_vertex_id() == m_task->game_vertex_id())
    {
        release_task();
        return false;
    }

    return true;
}

TEMPLATE_SPECIALIZATION
bool CStateMonsterSmartTerrainTaskGraphWalkAbstract::check_completion()
{
    if (CALifeSmartTerrainTask::audit_enabled())
        CALifeSmartTerrainTask::audit_use(m_task.get(), this, this->object->ID(), this->object->cName().c_str(),
            "graph_completion");
    // if we get to the graph point - work complete
    if (this->object->ai_location().game_vertex_id() == m_task->game_vertex_id())
        return true;
    return false;
}

TEMPLATE_SPECIALIZATION
void CStateMonsterSmartTerrainTaskGraphWalkAbstract::execute()
{
    if (CALifeSmartTerrainTask::audit_enabled())
        CALifeSmartTerrainTask::audit_use(m_task.get(), this, this->object->ID(), this->object->cName().c_str(),
            "graph_execute");
    this->object->set_action(ACT_WALK_FWD);
    this->object->set_state_sound(MonsterSound::eMonsterSoundIdle);

    this->object->path().detour_graph_points(m_task->game_vertex_id());
}

#undef TEMPLATE_SPECIALIZATION
#undef CStateMonsterSmartTerrainTaskGraphWalkAbstract
