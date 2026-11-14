#pragma once

#include <cstring>

namespace ragdoll_audit
{
// Read the current invocation, not a process-static cached launch option.
inline bool Enabled()
{
    const char* params = Core.Params;
    if (!params)
        return false;
    constexpr char flag[] = "-ragdoll_audit";
    for (const char* p = std::strstr(params, flag); p; p = std::strstr(p + 1, flag))
        if ((p == params || p[-1] == ' ' || p[-1] == '\t') &&
            (p[sizeof(flag) - 1] == '\0' || p[sizeof(flag) - 1] == ' ' || p[sizeof(flag) - 1] == '\t'))
            return true;
    return false;
}
}
