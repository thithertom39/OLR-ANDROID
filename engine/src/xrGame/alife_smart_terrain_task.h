////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_smart_terrain_task.h
//	Created 	: 20.09.2005
//  Modified 	: 20.09.2005
//	Author		: Dmitriy Iassenev
//	Description : ALife smart terrain task
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "xrAICore/Navigation/game_graph_space.h"

class CPatrolPoint;

class CALifeSmartTerrainTask
{
private:
#ifdef DEBUG
    shared_str m_patrol_path_name;
    u32 m_patrol_point_index;
#endif
    const CPatrolPoint* m_patrol_point;
    GameGraph::_GRAPH_ID m_game_vertex_id;
    u32 m_level_vertex_id;

#ifdef DEBUG
private:
    IC const shared_str& patrol_path_name() const;
    IC const u32& patrol_point_index() const;
#endif

private:
    void setup_patrol_point(const shared_str& patrol_path_name, const u32& patrol_point_index);
    IC const CPatrolPoint& patrol_point() const;
    IC void init(const shared_str& patrol_path_name, const u32& patrol_point_index);
    void audit_created(const char* constructor_kind);
    void audit_assigned();

public:
    CALifeSmartTerrainTask(LPCSTR patrol_path_name);
    CALifeSmartTerrainTask(LPCSTR patrol_path_name, const u32& patrol_point_index);
    CALifeSmartTerrainTask(const shared_str& patrol_path_name);
    CALifeSmartTerrainTask(const shared_str& patrol_path_name, const u32& patrol_point_index = 0);
    CALifeSmartTerrainTask(const GameGraph::_GRAPH_ID& game_vertex_id, const u32& level_vertex_id);
    CALifeSmartTerrainTask(const CALifeSmartTerrainTask& other);
    CALifeSmartTerrainTask& operator=(const CALifeSmartTerrainTask& other);
    ~CALifeSmartTerrainTask();
    GameGraph::_GRAPH_ID game_vertex_id() const;
    u32 level_vertex_id() const;
    Fvector position() const;

    // Diagnostic ownership observation only: no Lua reference or class-layout change.
    static bool audit_enabled();
    static void audit_use(const CALifeSmartTerrainTask* task, const void* owner_state, u16 owner_id,
        const char* owner_name, const char* stage, bool binding = false, u16 smart_terrain_id = u16(-1));

private:
    DECLARE_SCRIPT_REGISTER_FUNCTION();
};

#include "alife_smart_terrain_task_inline.h"
