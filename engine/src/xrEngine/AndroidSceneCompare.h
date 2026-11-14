#pragma once

#include <cstring>

namespace AndroidSceneCompare
{
template <std::size_t N>
bool HasOption(const char* args, const char (&option)[N])
{
    if (!args)
        return false;
    constexpr char whitespace[] = " \t\r\n";
    for (const char* match = args; (match = std::strstr(match, option)) != nullptr; ++match)
    {
        if ((match == args || std::strchr(whitespace, match[-1])) &&
            std::strchr(whitespace, match[N - 1]))
            return true;
    }
    return false;
}

inline bool Enabled(const char* args)
{
    return HasOption(args, "-gles_scene_compare");
}
} // namespace AndroidSceneCompare
