#pragma once

#include <algorithm>
#include <cmath>

namespace GLShadowVolumeClip
{
struct Bounds
{
    double low[3];
    double high[3];
};

template <typename Vertex>
Bounds MeshBounds(const Vertex* vertices, unsigned count)
{
    const auto& first = vertices[0];
    Bounds bounds{{first.x, first.y, first.z}, {first.x, first.y, first.z}};
    for (unsigned i = 1; i < count; ++i)
    {
        const double point[] = {vertices[i].x, vertices[i].y, vertices[i].z};
        for (unsigned j = 0; j < 3; ++j)
        {
            bounds.low[j] = std::min(bounds.low[j], point[j]);
            bounds.high[j] = std::max(bounds.high[j], point[j]);
        }
    }
    return bounds;
}

// Clip-space tests for w +/- x and w +/- y. All triangle vertices must lie
// outside the same plane. Using the local mesh AABB can only retain more draws.
inline bool Outside(const Bounds& bounds, const float (&wvp)[4][4])
{
    const double extent[] = {
        std::max(std::abs(bounds.low[0]), std::abs(bounds.high[0])),
        std::max(std::abs(bounds.low[1]), std::abs(bounds.high[1])),
        std::max(std::abs(bounds.low[2]), std::abs(bounds.high[2])),
    };
    for (unsigned axis = 0; axis < 2; ++axis)
    {
        double margin = 1.0 + std::abs(double(wvp[3][3])) + std::abs(double(wvp[3][axis]));
        double upper[] = {double(wvp[3][3]) - wvp[3][axis], double(wvp[3][3]) + wvp[3][axis]};
        for (unsigned j = 0; j < 3; ++j)
        {
            const double w = wvp[j][3], xy = wvp[j][axis];
            margin += (std::abs(w) + std::abs(xy)) * extent[j];
            const double minus = w - xy, plus = w + xy;
            upper[0] += minus * (minus > 0 ? bounds.high[j] : bounds.low[j]);
            upper[1] += plus * (plus > 0 ? bounds.high[j] : bounds.low[j]);
        }
        // Cover vertex shader float rounding around the plane boundary.
        if (std::isfinite(margin) && (upper[0] < -margin * 1.e-5 || upper[1] < -margin * 1.e-5))
            return true;
    }
    return false;
}
} // namespace GLShadowVolumeClip
