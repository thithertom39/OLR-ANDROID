#include "../engine/src/Layers/xrRenderGL/glStaticIndexBatchIndices.h"
#include <cassert>

int main()
{
    using xray::render::StaticBatchIndices::Append;
    std::vector<std::uint32_t> result;
    const std::uint16_t triangle[] = {0, 1, 2};
    assert(Append(result, triangle, 3, 65536, 3));
    assert(Append(result, triangle, 3, 17, 3));
    assert((result == std::vector<std::uint32_t>{65536, 65537, 65538, 17, 18, 19}));

    const auto original = result;
    const std::uint16_t restart[] = {65535, 0, 1};
    assert(!Append(result, restart, 3, 7, 65536));
    assert(!Append(result, triangle, 3, 7, 2));
    assert(!Append(result, triangle, 3, UINT32_MAX, 3));
    assert(!Append(result, triangle, 2, 7, 3));
    assert(result == original);
}
