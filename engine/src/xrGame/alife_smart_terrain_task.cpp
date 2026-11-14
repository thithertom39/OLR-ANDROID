////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_smart_terrain_task.cpp
//	Created 	: 20.09.2005
//  Modified 	: 20.09.2005
//	Author		: Dmitriy Iassenev
//	Description : ALife smart terrain task
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "alife_smart_terrain_task.h"
#include "alife_smart_terrain_task_owner.h"
#include "ai_space.h"
#include "xrServerEntities/xrServer_Objects_ALife.h"
#include "xrAICore/Navigation/PatrolPath/patrol_path_storage.h"
#include "xrAICore/Navigation/PatrolPath/patrol_path.h"
#include "xrAICore/Navigation/PatrolPath/patrol_point.h"

#include "xrAICore/Navigation/level_graph.h"
#include "xrAICore/Navigation/game_graph.h"

class CSmartTerrainTaskOwner::ResetCallback : public CEventNotifierCallbackWithCid
{
    CSmartTerrainTaskOwner* m_holder;

public:
    ResetCallback(CID cid, CSmartTerrainTaskOwner* holder)
        : CEventNotifierCallbackWithCid(cid), m_holder(holder)
    {
    }

    void ProcessEvent() override { m_holder->on_script_engine_reset(); }
};

CSmartTerrainTaskOwner::~CSmartTerrainTaskOwner()
{
    reset();
    if (m_reset_space && m_reset_cid != CEventNotifierCallback::INVALID_CID)
        m_reset_space->Unsubscribe(m_reset_cid, CAI_Space::EVENT_SCRIPT_ENGINE_RESET);
}

void CSmartTerrainTaskOwner::reset()
{
    m_task = nullptr;
    luabind::object empty;
    m_owner.swap(empty);
}

void CSmartTerrainTaskOwner::on_script_engine_reset()
{
    // CAI_Space emits this event before reinitializing or closing its Lua VM.
    reset();
    CAI_Space* space = m_reset_space;
    const auto cid = m_reset_cid;
    m_reset_space = nullptr;
    m_reset_cid = CEventNotifierCallback::INVALID_CID;
    if (space && cid != CEventNotifierCallback::INVALID_CID)
        space->Unsubscribe(cid, CAI_Space::EVENT_SCRIPT_ENGINE_RESET);
}

void CSmartTerrainTaskOwner::assign(CALifeSmartTerrainTask* task, luabind::object& owner)
{
    if (!task)
    {
        reset();
        return;
    }
    if (owner.is_valid() && !m_reset_space)
    {
        CAI_Space& space = ai();
        const auto cid = space.Subscribe<ResetCallback>(CAI_Space::EVENT_SCRIPT_ENGINE_RESET, this);
        m_reset_space = &space;
        m_reset_cid = cid;
    }
    m_task = task;
    m_owner.swap(owner);
}

CALifeSmartTerrainTask* CSmartTerrainTaskOwner::acquire(CSE_ALifeSmartZone& zone, CSE_ALifeMonsterAbstract* monster)
{
    luabind::object owner;
    CALifeSmartTerrainTask* task = zone.task_with_owner(monster, owner);
    assign(task, owner);
    return m_task;
}

#ifdef XR_PLATFORM_ANDROID
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <mutex>

#if defined(__aarch64__) && !defined(DEBUG)
static_assert(sizeof(CALifeSmartTerrainTask) == 16, "AI task audit must preserve the ARM64 Release layout");
#endif

namespace
{
constexpr size_t taskAuditCapacity = 4096;
constexpr size_t taskAuditOwnerCapacity = 512;
constexpr size_t taskAuditProbeLimit = 32;
constexpr size_t taskAuditEventCapacity = 128;

struct TaskAuditFields
{
    const CPatrolPoint* patrol_point = nullptr;
    GameGraph::_GRAPH_ID game_vertex = GameGraph::_GRAPH_ID(-1);
    u32 level_vertex = u32(-1);
};

struct TaskAuditRecord
{
    const void* key = nullptr;
    u64 generation = 0;
    bool alive = false;
    const char* constructor_kind = "unknown";
    TaskAuditFields fields;
};

struct TaskAuditOwner
{
    const void* key = nullptr;
    const CALifeSmartTerrainTask* task = nullptr;
    u64 generation = 0;
    u16 owner_id = u16(-1);
    u16 smart_terrain_id = u16(-1);
};

struct TaskAuditEvent
{
    u64 sequence = 0;
    u64 generation = 0;
    const void* task = nullptr;
    const char* kind = "unknown";
    const char* constructor_kind = "unknown";
    TaskAuditFields fields;
    u16 owner_id = u16(-1);
    u16 smart_terrain_id = u16(-1);
};

std::mutex taskAuditMutex;
std::array<TaskAuditRecord, taskAuditCapacity> taskAuditRecords;
std::array<TaskAuditOwner, taskAuditOwnerCapacity> taskAuditOwners;
std::array<TaskAuditEvent, taskAuditEventCapacity> taskAuditEvents;
u64 taskAuditNextGeneration = 0;
u64 taskAuditNextEvent = 0;
u64 taskAuditOverflows = 0;
u32 taskAuditLifecycleLogs = 0;
u32 taskAuditOwnerLogs = 0;
u32 taskAuditUnknownLogs = 0;

// Never evict an existing address: eviction could turn incomplete coverage into
// a false lifetime claim. Full/probe-limited tables report unknown instead.
template <typename Entry, size_t Capacity>
Entry* FindTaskAuditEntry(std::array<Entry, Capacity>& entries, const void* key, bool insert)
{
    static_assert((Capacity & (Capacity - 1)) == 0, "Audit capacity must be a power of two");
    if (!key)
        return nullptr;

    auto hash = reinterpret_cast<std::uintptr_t>(key) >> 4;
    hash ^= hash >> 17;
    hash ^= hash >> 9;
    for (size_t probe = 0; probe < taskAuditProbeLimit; ++probe)
    {
        Entry& entry = entries[(hash + probe) & (Capacity - 1)];
        if (entry.key == key)
            return &entry;
        if (!entry.key)
        {
            if (insert)
                entry.key = key;
            return insert ? &entry : nullptr;
        }
    }
    return nullptr;
}

TaskAuditEvent AddTaskAuditEvent(const TaskAuditRecord& record, const char* kind,
    u16 owner_id = u16(-1), u16 smart_terrain_id = u16(-1))
{
    TaskAuditEvent event;
    event.sequence = ++taskAuditNextEvent;
    event.generation = record.generation;
    event.task = record.key;
    event.kind = kind;
    event.constructor_kind = record.constructor_kind;
    event.fields = record.fields;
    event.owner_id = owner_id;
    event.smart_terrain_id = smart_terrain_id;
    taskAuditEvents[(event.sequence - 1) % taskAuditEventCapacity] = event;
    return event;
}

void LogTaskAuditEvent(const TaskAuditEvent& event)
{
    Msg("* [ai_task_audit] event=%llu kind=%s task=%p generation=%llu ctor=%s "
        "patrol=%p graph=%u level=%u owner=%u smart=%u",
        static_cast<unsigned long long>(event.sequence), event.kind, event.task,
        static_cast<unsigned long long>(event.generation), event.constructor_kind,
        static_cast<const void*>(event.fields.patrol_point), static_cast<unsigned>(event.fields.game_vertex),
        event.fields.level_vertex, static_cast<unsigned>(event.owner_id), static_cast<unsigned>(event.smart_terrain_id));
}

void DumpTaskAuditEvents()
{
    std::array<TaskAuditEvent, taskAuditEventCapacity> events;
    size_t count;
    u64 overflows;
    {
        const std::lock_guard<std::mutex> lock(taskAuditMutex);
        count = static_cast<size_t>(std::min<u64>(taskAuditNextEvent, taskAuditEventCapacity));
        const u64 first = taskAuditNextEvent - count;
        for (size_t i = 0; i < count; ++i)
            events[i] = taskAuditEvents[(first + i) % taskAuditEventCapacity];
        overflows = taskAuditOverflows;
    }
    Msg("! [ai_task_audit] history count=%zu coverage_overflows=%llu (untracked addresses are unknown)",
        count, static_cast<unsigned long long>(overflows));
    for (size_t i = 0; i < count; ++i)
        LogTaskAuditEvent(events[i]);
    FlushLog();
}
} // namespace
#endif // XR_PLATFORM_ANDROID

bool CALifeSmartTerrainTask::audit_enabled()
{
#ifdef XR_PLATFORM_ANDROID
    if (!Core.Params)
        return false;

    // Command-line switches are fixed for this native process. Do not cache a
    // false value during static initialization, before Core.Params is populated.
    static const bool enabled = []()
    {
        constexpr char option[] = "-ai_task_audit";
        const char* cursor = Core.Params;
        while (*cursor)
        {
            while (*cursor && std::isspace(static_cast<unsigned char>(*cursor)))
                ++cursor;
            const char* token = cursor;
            bool quoted = false;
            while (*cursor && (quoted || !std::isspace(static_cast<unsigned char>(*cursor))))
            {
                if (*cursor == '"')
                    quoted = !quoted;
                ++cursor;
            }
            size_t length = static_cast<size_t>(cursor - token);
            if (length >= 2 && token[0] == '"' && cursor[-1] == '"')
            {
                ++token;
                length -= 2;
            }
            if (!quoted && length == sizeof(option) - 1 && !memcmp(token, option, length))
                return true;
        }
        return false;
    }();
    return enabled;
#else
    return false;
#endif
}

void CALifeSmartTerrainTask::audit_created(const char* constructor_kind)
{
#ifdef XR_PLATFORM_ANDROID
    if (!audit_enabled())
        return;

    TaskAuditEvent event;
    bool log_event;
    bool log_overflow = false;
    {
        const std::lock_guard<std::mutex> lock(taskAuditMutex);
        TaskAuditRecord snapshot;
        snapshot.key = this;
        snapshot.generation = ++taskAuditNextGeneration;
        snapshot.alive = true;
        snapshot.constructor_kind = constructor_kind;
        snapshot.fields = {m_patrol_point, m_game_vertex_id, m_level_vertex_id};
        if (TaskAuditRecord* record = FindTaskAuditEntry(taskAuditRecords, this, true))
            *record = snapshot;
        else
        {
            log_overflow = ++taskAuditOverflows <= 4;
            // There is an event, but no retained lifetime entry for this object.
            snapshot.generation = 0;
        }
        event = AddTaskAuditEvent(snapshot, "create");
        log_event = taskAuditLifecycleLogs < 64;
        if (log_event)
            ++taskAuditLifecycleLogs;
    }
    if (log_event)
        LogTaskAuditEvent(event);
    if (log_overflow)
        Msg("! [ai_task_audit] tracking capacity/probe limit reached; task=%p has unknown lifetime",
            static_cast<const void*>(this));
#else
    (void)constructor_kind;
#endif
}

void CALifeSmartTerrainTask::audit_assigned()
{
#ifdef XR_PLATFORM_ANDROID
    if (!audit_enabled())
        return;

    TaskAuditEvent event;
    bool log_event;
    {
        const std::lock_guard<std::mutex> lock(taskAuditMutex);
        TaskAuditRecord snapshot;
        snapshot.key = this;
        snapshot.fields = {m_patrol_point, m_game_vertex_id, m_level_vertex_id};
        if (TaskAuditRecord* record = FindTaskAuditEntry(taskAuditRecords, this, false))
        {
            // Assignment changes values, not the lifetime/generation of the target.
            record->fields = snapshot.fields;
            record->constructor_kind = "assignment";
            snapshot.generation = record->generation;
            snapshot.constructor_kind = record->constructor_kind;
        }
        event = AddTaskAuditEvent(snapshot, "assign");
        log_event = taskAuditLifecycleLogs < 64;
        if (log_event)
            ++taskAuditLifecycleLogs;
    }
    if (log_event)
        LogTaskAuditEvent(event);
#endif
}

CALifeSmartTerrainTask::~CALifeSmartTerrainTask()
{
#ifdef XR_PLATFORM_ANDROID
    if (!audit_enabled())
        return;

    TaskAuditEvent event;
    bool log_event;
    {
        const std::lock_guard<std::mutex> lock(taskAuditMutex);
        TaskAuditRecord snapshot;
        snapshot.key = this;
        // Destruction has begun, but the object's own fields still exist.
        snapshot.fields = {m_patrol_point, m_game_vertex_id, m_level_vertex_id};
        if (TaskAuditRecord* record = FindTaskAuditEntry(taskAuditRecords, this, false))
        {
            record->alive = false;
            snapshot.generation = record->generation;
            snapshot.constructor_kind = record->constructor_kind;
        }
        event = AddTaskAuditEvent(snapshot, "destroy");
        log_event = taskAuditLifecycleLogs < 64;
        if (log_event)
            ++taskAuditLifecycleLogs;
    }
    if (log_event)
        LogTaskAuditEvent(event);
#endif
}

void CALifeSmartTerrainTask::audit_use(const CALifeSmartTerrainTask* task, const void* owner_state, u16 owner_id,
    const char* owner_name, const char* stage, bool binding, u16 smart_terrain_id)
{
#ifdef XR_PLATFORM_ANDROID
    if (!audit_enabled())
        return;

    const char* failure = nullptr;
    TaskAuditRecord snapshot;
    TaskAuditFields current;
    TaskAuditEvent owner_event;
    bool log_owner = false;
    bool log_unknown = false;
    u64 expected_generation = 0;
    {
        const std::lock_guard<std::mutex> lock(taskAuditMutex);
        // Look up the address first; a recorded dead task is never dereferenced.
        TaskAuditRecord* record = FindTaskAuditEntry(taskAuditRecords, task, false);
        snapshot.key = task;
        if (record)
            snapshot = *record;

        if (owner_state)
        {
            TaskAuditOwner* owner = FindTaskAuditEntry(taskAuditOwners, owner_state, true);
            if (!owner)
                ++taskAuditOverflows;
            else
            {
                const bool new_binding = binding || owner->owner_id != owner_id;
                if (new_binding)
                {
                    const bool changed = owner->task != task || owner->generation != snapshot.generation ||
                        owner->owner_id != owner_id || owner->smart_terrain_id != smart_terrain_id;
                    owner->task = task;
                    owner->generation = snapshot.generation;
                    owner->owner_id = owner_id;
                    owner->smart_terrain_id = smart_terrain_id;
                    if (changed)
                    {
                        owner_event = AddTaskAuditEvent(snapshot, stage, owner_id, smart_terrain_id);
                        log_owner = taskAuditOwnerLogs < 32;
                        if (log_owner)
                            ++taskAuditOwnerLogs;
                    }
                }
                if (owner->task == task)
                    expected_generation = owner->generation;
                if (smart_terrain_id == u16(-1))
                    smart_terrain_id = owner->smart_terrain_id;
            }
        }

        if (!task)
            failure = "null_task";
        else if (!record)
        {
            // Coverage loss must not become a use-after-free diagnosis or fallback.
            log_unknown = taskAuditUnknownLogs < 8;
            if (log_unknown)
                ++taskAuditUnknownLogs;
        }
        else if (!record->alive)
            failure = "recorded_dead_task";
        else if (expected_generation && expected_generation != record->generation)
            failure = "task_address_reused_since_binding";
        else
        {
            current = {task->m_patrol_point, task->m_game_vertex_id, task->m_level_vertex_id};
            if (current.patrol_point != record->fields.patrol_point ||
                current.game_vertex != record->fields.game_vertex || current.level_vertex != record->fields.level_vertex)
                failure = "live_task_fields_changed";
            else if (!current.patrol_point && current.game_vertex == GameGraph::_GRAPH_ID(-1))
                failure = "invalid_graph_without_patrol_point";
        }
    }
    if (log_owner)
    {
        LogTaskAuditEvent(owner_event);
        Msg("* [ai_task_audit] owner=%u name=%s state=%p stage=%s task=%p",
            static_cast<unsigned>(owner_id), owner_name ? owner_name : "unknown", owner_state, stage,
            static_cast<const void*>(task));
    }
    if (log_unknown)
        Msg("! [ai_task_audit] lifetime=unknown task=%p owner=%u name=%s stage=%s; no lifetime verdict",
            static_cast<const void*>(task), static_cast<unsigned>(owner_id), owner_name ? owner_name : "unknown", stage);
    if (failure)
    {
        Msg("! [ai_task_audit] failure=%s task=%p generation=%llu expected_generation=%llu alive=%u "
            "ctor=%s owner=%u name=%s smart=%u stage=%s",
            failure, static_cast<const void*>(task), static_cast<unsigned long long>(snapshot.generation),
            static_cast<unsigned long long>(expected_generation), static_cast<unsigned>(snapshot.alive),
            snapshot.constructor_kind, static_cast<unsigned>(owner_id), owner_name ? owner_name : "unknown",
            static_cast<unsigned>(smart_terrain_id), stage);
        Msg("! [ai_task_audit] snapshot patrol=%p graph=%u level=%u; current patrol=%p graph=%u level=%u "
            "(current fields unavailable for dead/unknown/reused tasks)",
            static_cast<const void*>(snapshot.fields.patrol_point), static_cast<unsigned>(snapshot.fields.game_vertex),
            snapshot.fields.level_vertex, static_cast<const void*>(current.patrol_point),
            static_cast<unsigned>(current.game_vertex), current.level_vertex);
        DumpTaskAuditEvents();
        FATAL_F("[ai_task_audit] %s task=%p owner=%u stage=%s", failure, static_cast<const void*>(task),
            static_cast<unsigned>(owner_id), stage);
        // Fatal() may allow a debugger to continue; never resume a known-invalid read.
        std::abort();
    }
#else
    (void)task;
    (void)owner_state;
    (void)owner_id;
    (void)owner_name;
    (void)stage;
    (void)binding;
    (void)smart_terrain_id;
#endif
}

//#define ENABLE_MISSING_PATROL_PATH_WORKAROUNDS

#ifdef ENABLE_MISSING_PATROL_PATH_WORKAROUNDS
// workarounds for mods
static const CPatrolPath* TryToNotCrash(const shared_str& patrol_path_name)
{
    const CPatrolPath* patrol_path = nullptr;

    const size_t sz = patrol_path_name.size();
    pcstr path_name = patrol_path_name.c_str();
    pcstr path_name_end = path_name + sz;
    Msg("! Cannot find %s. Executing workarounds...", path_name);
    do
    {
        // Imagine we have missing patrol path mil_monster_burrow_29_home_3
        // Try to fallback to mil_monster_burrow_29_home_2
        char const* almostEnd = strrchr(path_name, '_');
        if (!almostEnd || (++almostEnd == path_name_end))
            break;
        int number = strtol(almostEnd, nullptr, 10);
        if (number == 0 || errno == ERANGE)
            break;
        xr_string temp(path_name, almostEnd);
        temp += std::to_string(--number);

        patrol_path = const_cast<CPatrolPathStorage&>(ai().patrol_paths()).add_alias_if_exist(temp.c_str(), patrol_path_name);
    } while (false);

    if (!patrol_path)
    {
        Msg("! ... Workarounds didn't help.", path_name);
        // Let it crash.
        patrol_path = ai().patrol_paths().path(patrol_path_name); // the call is the same as in GetPatrolPath, but with assertion enabled
    }
    return patrol_path;
}

static const CPatrolPath* GetPatrolPath(const shared_str& patrol_path_name)
{
    const CPatrolPath* patrol_path = ai().patrol_paths().path(patrol_path_name, true); // assertion on error disabled
    VERIFY(patrol_path);
    if (!patrol_path) // engage workarounds
        patrol_path = TryToNotCrash(patrol_path_name);
    return patrol_path;
}

#else
static const CPatrolPath* GetPatrolPath(const shared_str& patrol_path_name)
{
    return ai().patrol_paths().path(patrol_path_name);
}
#endif // ENABLE_MISSING_PATROL_PATH_WORKAROUNDS

void CALifeSmartTerrainTask::setup_patrol_point(const shared_str& patrol_path_name, const u32& patrol_point_index)
{
    VERIFY(!m_patrol_point);

    const CPatrolPath* patrol_path = GetPatrolPath(patrol_path_name);
    VERIFY(patrol_path);

    m_patrol_point = &patrol_path->vertex(patrol_point_index)->data();
    VERIFY(m_patrol_point);
}

GameGraph::_GRAPH_ID CALifeSmartTerrainTask::game_vertex_id() const
{
    audit_use(this, nullptr, u16(-1), nullptr, "game_vertex_id");
    if (m_game_vertex_id == GameGraph::_GRAPH_ID(-1))
    {
        VERIFY3(ai().game_graph().valid_vertex_id(patrol_point().game_vertex_id()), m_patrol_path_name.c_str(),
            m_patrol_point->name().c_str());
        return (patrol_point().game_vertex_id());
    }
    else
    {
        VERIFY(ai().game_graph().valid_vertex_id(m_game_vertex_id));
        return m_game_vertex_id;
    }
}

u32 CALifeSmartTerrainTask::level_vertex_id() const
{
    audit_use(this, nullptr, u16(-1), nullptr, "level_vertex_id");
    if (m_level_vertex_id == u32(-1))
    {
        VERIFY3(ai().game_graph().valid_vertex_id(patrol_point().game_vertex_id()), m_patrol_path_name.c_str(),
            m_patrol_point->name().c_str());
        return (patrol_point().level_vertex_id());
    }
    else
    {
        VERIFY2(ai().game_graph().valid_vertex_id(m_game_vertex_id),
            make_string("Vertex [%d] is not valid!!!", m_game_vertex_id));
        return m_level_vertex_id;
    }
}

Fvector CALifeSmartTerrainTask::position() const
{
    audit_use(this, nullptr, u16(-1), nullptr, "position");
    if (m_level_vertex_id == u32(-1))
    {
        return (patrol_point().position());
    }
    else
    {
        if (ai().game_graph().vertex(m_game_vertex_id)->level_id() == ai().level_graph().level_id())
            return (ai().level_graph().vertex_position(m_level_vertex_id));
        else
            return ai().game_graph().vertex(m_game_vertex_id)->level_point();
    }
}
