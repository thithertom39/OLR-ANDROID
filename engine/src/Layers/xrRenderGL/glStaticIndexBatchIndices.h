#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace xray::render::StaticBatchIndices
{
inline bool Append(std::vector<std::uint32_t>& output, const std::uint16_t* source,
    std::size_t count, std::uint32_t base, std::uint32_t vertexCount)
{
    if (!source || count % 3 || base > std::numeric_limits<std::uint32_t>::max() - 65535u)
        return false;
    const auto initialSize = output.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        // A restart can leave a partial GL_TRIANGLES primitive. It must not
        // be completed by indices from the next draw. Use the original draw
        // when the reserved value occurs, including with restart disabled.
        if (source[i] == 65535u || source[i] >= vertexCount)
        {
            output.resize(initialSize);
            return false;
        }
        output.push_back(base + source[i]);
    }
    return true;
}
}
