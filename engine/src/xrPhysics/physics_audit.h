#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include "xrPhysics.h"

// Optional diagnostics only. No solver, lifetime, or global statistics settings
// are changed. The small policy helpers are independent of game/clock state.
namespace physics_audit
{
inline bool HasFlag(const char* arguments)
{
    if (!arguments)
        return false;
    constexpr const char flag[] = "-physics_audit";
    for (const char* at = std::strstr(arguments, flag); at; at = std::strstr(at + 1, flag))
    {
        const char after = at[sizeof(flag) - 1];
        if ((at == arguments || std::isspace(static_cast<unsigned char>(at[-1]))) &&
            (!after || std::isspace(static_cast<unsigned char>(after))))
            return true;
    }
    return false;
}

class Budget
{
    std::uint32_t remaining;
    std::uint64_t intervalNs;
    std::uint64_t lastNs = 0;
    bool started = false;

public:
    constexpr Budget(std::uint32_t count, std::uint64_t interval) : remaining(count), intervalNs(interval) {}
    bool Pending() const { return remaining != 0; }
    bool Take(std::uint64_t nowNs)
    {
        if (!remaining)
            return false;
        // steady_clock should not go backwards. If a host/test clock resets,
        // rearm the interval without restoring the exhausted line budget.
        if (started && nowNs >= lastNs && nowNs - lastNs < intervalNs)
            return false;
        lastNs = nowNs;
        started = true;
        --remaining;
        return true;
    }
};

inline std::uint64_t SaturatingAdd(std::uint64_t value, std::uint64_t increment)
{
    const auto maximum = ~std::uint64_t(0);
    return increment > maximum - value ? maximum : value + increment;
}

XRPHYSICS_API bool Enabled();
XRPHYSICS_API void RecordFragment(std::uint16_t sourceId, std::uint16_t fragmentId,
    const char* section, std::uint32_t timeMs, std::uint32_t defaultTimeMs, bool explicitTime);
}
