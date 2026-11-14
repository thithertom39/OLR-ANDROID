#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>

// Opt-in diagnostics for the existing partner-inventory filter. The UI and
// CanTrade run on the game thread; never evaluate an NPC's sell policy twice.
namespace TradeAudit
{
constexpr unsigned snapshotLimit = 8;
constexpr unsigned itemLimit = 32;

struct ItemResult
{
    std::uint16_t id{};
    bool tested{};
    bool allowed{};
    bool ownerAllowed{};
    bool tradeFlag{};
    bool quest{};
    unsigned sellRestriction{}; // 0: none, 1: local sell rule, 2: global sell rule
};

struct State
{
    unsigned snapshots{};
    bool active{};
    std::uint16_t owner{};
    unsigned watched{};
    unsigned tested{};
    unsigned allowed{};
    unsigned ownerDenied{};
    unsigned flagDenied{};
    unsigned questDenied{};
    std::array<ItemResult, itemLimit> items{};
};

inline thread_local State state;

inline bool HasOption(const char* params)
{
    if (!params)
        return false;
    constexpr char option[] = "-trade_audit";
    for (const char* p = std::strstr(params, option); p; p = std::strstr(p + 1, option))
    {
        const bool start = p == params || std::isspace(static_cast<unsigned char>(p[-1]));
        const char tail = p[sizeof(option) - 1];
        if (start && (tail == '\0' || std::isspace(static_cast<unsigned char>(tail))))
            return true;
    }
    return false;
}

inline bool ActiveFor(std::uint16_t owner) { return state.active && state.owner == owner; }

inline void Record(std::uint16_t owner, std::uint16_t id, bool allowed, bool ownerAllowed,
    bool tradeFlag, bool quest, unsigned sellRestriction)
{
    if (!ActiveFor(owner))
        return;
    ++state.tested;
    state.allowed += allowed;
    state.ownerDenied += !ownerAllowed;
    state.flagDenied += !tradeFlag;
    state.questDenied += quest;
    for (unsigned i = 0; i < state.watched; ++i)
    {
        if (state.items[i].id == id)
        {
            state.items[i] = { id, true, allowed, ownerAllowed, tradeFlag, quest, sellRestriction };
            break;
        }
    }
}

class Snapshot
{
    bool m_active{};

public:
    Snapshot(const char* params, std::uint16_t owner)
    {
        if (state.active || state.snapshots >= snapshotLimit || !HasOption(params))
            return;
        const unsigned snapshots = state.snapshots + 1;
        state = {};
        state.snapshots = snapshots;
        state.owner = owner;
        state.active = m_active = true;
    }

    ~Snapshot()
    {
        if (!m_active)
            return;
        state.active = false;
        if (state.snapshots == snapshotLimit)
            Msg("* [trade_audit] snapshot limit reached (%u); further trade refreshes are silent", snapshotLimit);
    }

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    explicit operator bool() const { return m_active; }

    void Watch(std::uint16_t id)
    {
        if (m_active && state.watched < itemLimit)
            state.items[state.watched++].id = id;
    }

    void LogItem(std::uint16_t id, const char* section, unsigned place, unsigned slot,
        unsigned baseSlot, bool available, bool queued)
    {
        if (!m_active)
            return;
        for (unsigned i = 0; i < state.watched; ++i)
        {
            const auto& result = state.items[i];
            if (result.id != id)
                continue;
            Msg("* [trade_audit] snapshot=%u owner=%u item=%u section=%s place=%u slot=%u base_slot=%u "
                "tested=%u can_trade=%u owner_allows=%u trade_flag=%u quest=%u sell_restriction=%u available=%u queued=%u",
                state.snapshots, state.owner, id, section, place, slot, baseSlot, result.tested, result.allowed,
                result.ownerAllowed, result.tradeFlag, result.quest, result.sellRestriction, available, queued);
            return;
        }
    }
};
}
