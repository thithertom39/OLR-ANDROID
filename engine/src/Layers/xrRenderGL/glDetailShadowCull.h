#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

namespace xray::render::RENDER_NAMESPACE::GLDetailShadowCull
{
// Program names are vs|ps|gs, with an optional built-in skin index on vs.
// Unknown detail vertex programs keep their original draw unconditionally.
inline bool KnownProgram(const char* name, bool animated)
{
    if (!name) return false;
    const char* prefix = animated ? "deffer_detail_w_flat" : "deffer_detail_s_flat";
    const std::size_t length = std::strlen(prefix);
    if (std::strncmp(name, prefix, length) != 0) return false;
    const char* suffix = name + length;
    return suffix[0] == '|' ||
        (suffix[0] == '_' && suffix[1] >= '0' && suffix[1] <= '4' && suffix[2] == '|');
}

template<std::size_t N>
bool HasOption(const char* args, const char (&flag)[N])
{
    if (!args) return false;
    for (const char* p = args; (p = std::strstr(p, flag)) != nullptr; ++p)
        if ((p == args || std::strchr(" \t\r\n", p[-1])) &&
            (!p[N - 1] || std::strchr(" \t\r\n", p[N - 1]))) return true;
    return false;
}

inline bool EagerSetupRequested(const char* args)
{
    return HasOption(args, "-gles_detail_eager_setup");
}

inline bool CellCacheEnabled(const char* args)
{
    // Exact negative flag allows same-binary A/B. The old positive flag remains
    // compatible. Bounds live only for one immutable light-package traversal.
    return !HasOption(args, "-gles_no_detail_cells");
}

// No allocation, no cached pointers: same immutable visible list as the draw.
// An invalid bound makes the original predicate retain the instance.
template<class Groups, class Predicate>
bool AnySurvivor(const Groups& groups, Predicate visible)
{
    for (auto items : groups)
        for (const auto& instance : *items)
            if (visible(instance)) return true;
    return false;
}

struct Bounds
{
    std::array<double, 3> center{}, extent{};
    bool valid = false;
};

// The standard detail shader transforms the model box with the very same
// three packed rows. Its wave is horizontal: wind * height * cyclic * frac.
// cyclic is bounded by 1.0001 (including float rounding), and packed signed
// SHORT/16384 makes abs(frac) <= 2. No camera-distance approximation is used.
template <class Box, class Matrix, class Vector>
Bounds Build(const Box& box, const Matrix& matrix, float scale,
             const Vector& wind, const Vector& wave, bool animated)
{
    Bounds out;
    const double lo[3] = {box.vMin.x, box.vMin.y, box.vMin.z};
    const double hi[3] = {box.vMax.x, box.vMax.y, box.vMax.z};
    const double translation[3] = {matrix._41, matrix._42, matrix._43};
    const double rows[3][3] = {
        {matrix._11, matrix._21, matrix._31},
        {matrix._12, matrix._22, matrix._32},
        {matrix._13, matrix._23, matrix._33},
    };
    const double windValues[4] = {wind.x, wind.y, wind.z, wind.w};
    const double waveValues[4] = {wave.x, wave.y, wave.z, wave.w};
    if (!std::isfinite(scale)) return out;
    for (unsigned i = 0; i != 3; ++i)
    {
        if (!std::isfinite(lo[i]) || !std::isfinite(hi[i]) || lo[i] > hi[i] ||
            !std::isfinite(translation[i])) return out;
        for (unsigned j = 0; j != 3; ++j)
            if (!std::isfinite(rows[i][j])) return out;
    }
    if (animated)
        for (unsigned i = 0; i != 4; ++i)
            if (!std::isfinite(windValues[i]) || !std::isfinite(waveValues[i])) return out;
    for (unsigned i = 0; i != 3; ++i)
    {
        out.center[i] = translation[i];
        for (unsigned j = 0; j != 3; ++j)
        {
            const double value = rows[i][j] * double(scale);
            out.center[i] += value * ((lo[j] + hi[j]) * 0.5);
            out.extent[i] += std::fabs(value) * ((hi[j] - lo[j]) * 0.5);
        }
    }
    if (animated)
    {
        const double height = std::fabs(out.center[1] - translation[1]) + out.extent[1];
        out.extent[0] += std::fabs(windValues[0]) * height * 2.0002;
        out.extent[2] += std::fabs(windValues[2]) * height * 2.0002;
    }
    // Keep numerical boundary cases. This deliberately exceeds the error of
    // float row packing, shader arithmetic and frustum extraction by a wide
    // margin; very large world coordinates only make culling more permissive.
    double magnitude = 1.0;
    for (unsigned i = 0; i != 3; ++i)
    {
        magnitude += std::fabs(out.center[i]) + out.extent[i] + std::fabs(translation[i]);
        // Include individual products: a small final center can result from
        // cancellation of large terms in the shader's float dot product.
        for (unsigned j = 0; j != 3; ++j)
            magnitude += std::fabs(rows[i][j] * double(scale)) *
                (std::fabs(lo[j]) > std::fabs(hi[j]) ? std::fabs(lo[j]) : std::fabs(hi[j]));
    }
    if (!std::isfinite(magnitude) || magnitude > double(std::numeric_limits<float>::max())/16.0)
        return Bounds{};
    const double margin = 0.001 + 0.0001 * magnitude;
    for (unsigned i = 0; i != 3; ++i)
    {
        out.extent[i] += margin;
        if (!std::isfinite(out.center[i]) || !std::isfinite(out.extent[i])) return Bounds{};
    }
    out.valid = true;
    return out;
}

// Scoped to one render_lights package. No frame-number/pointer cache survives
// a package, a level reload, or a new visibility calculation.
struct BoundsBatch;
inline thread_local BoundsBatch* activeBoundsBatch = nullptr;
// Reuse the allocation between packages. Only the outer package owns it and
// only entries written by that package are readable.
inline constexpr std::size_t MaxCachedCells = 16384;
inline thread_local std::unique_ptr<Bounds[]> cellScratch;

struct ModelBounds
{
    Bounds envelope;
    const Bounds* cells = nullptr;
};

struct BoundsBatch
{
    const void* owner;
    BoundsBatch* previous;
    const bool cacheCells;
    std::size_t cellsUsed = 0;
    std::array<ModelBounds, 3 * 64> entries{};
    std::array<bool, 3 * 64> ready{};
    explicit BoundsBatch(const void* value, bool useCells = false)
        : owner(value), previous(activeBoundsBatch), cacheCells(useCells && !previous)
    {
        activeBoundsBatch = this;
    }
    ~BoundsBatch() { activeBoundsBatch=previous; }
    BoundsBatch(const BoundsBatch&)=delete;
    BoundsBatch& operator=(const BoundsBatch&)=delete;
};
template<class Builder>
ModelBounds BatchBounds(const void* owner, unsigned variant, unsigned model, std::size_t cellCount, Builder build)
{
    auto* batch=activeBoundsBatch;
    if (!batch || batch->owner!=owner || variant>=3 || model>=64) return {};
    const unsigned index=variant*64+model;
    auto& entry = batch->entries[index];
    if (!batch->ready[index])
    {
        Bounds* cells = nullptr;
        if (batch->cacheCells && cellCount && cellCount <= MaxCachedCells - batch->cellsUsed)
        {
            if (!cellScratch)
                cellScratch.reset(new (std::nothrow) Bounds[MaxCachedCells]);
            if (cellScratch)
                cells = cellScratch.get() + batch->cellsUsed;
        }
        entry.envelope = build(cells);
        if (cells && entry.envelope.valid)
        {
            entry.cells = cells;
            batch->cellsUsed += cellCount;
        }
        batch->ready[index] = true;
    }
    return entry;
}
inline Bounds UnionBounds(const Bounds& a, const Bounds& b)
{
    Bounds out;
    if (!a.valid || !b.valid) return out;
    for (unsigned i=0;i<3;++i)
    {
        const double lo=std::fmin(a.center[i]-a.extent[i],b.center[i]-b.extent[i]);
        const double hi=std::fmax(a.center[i]+a.extent[i],b.center[i]+b.extent[i]);
        out.center[i]=(lo+hi)*0.5;
        out.extent[i]=(hi-lo)*0.5 + 0.00001*(1.0+std::fabs(lo)+std::fabs(hi));
        if (!std::isfinite(out.center[i]) || !std::isfinite(out.extent[i])) return Bounds{};
    }
    out.valid=true;return out;
}

template<class Groups, class BuildInstance>
Bounds BuildModelBounds(const Groups& groups, Bounds* cells, BuildInstance build)
{
    Bounds model;
    for (const auto* items : groups)
    {
        Bounds cell;
        for (const auto* instance : *items)
        {
            const auto bound = build(*instance);
            if (!bound.valid)
                return {};
            model = model.valid ? UnionBounds(model, bound) : bound;
            if (!model.valid)
                return {};
            if (cells)
                cell = cell.valid ? UnionBounds(cell, bound) : bound;
        }
        if (cells)
        {
            // Give even a single-instance cell an outward rounding margin.
            *cells++ = UnionBounds(cell, cell);
        }
    }
    return model;
}

template<class Vector>
bool WindWithinEnvelope(float limit, const Vector& wind, const Vector& wave)
{
    return std::isfinite(wind.x) && std::isfinite(wind.y) &&
        std::isfinite(wind.z) && std::isfinite(wind.w) &&
        std::isfinite(wave.x) && std::isfinite(wave.y) &&
        std::isfinite(wave.z) && std::isfinite(wave.w) &&
        std::fabs(wind.x) <= limit && std::fabs(wind.z) <= limit;
}

template <class Plane>
bool ValidPlanes(const Plane* planes, std::size_t count)
{
    if (!planes || count == 0 || count > 12) return false;
    for (std::size_t i = 0; i != count; ++i)
    {
        const auto& p = planes[i];
        if (!std::isfinite(p.n.x) || !std::isfinite(p.n.y) || !std::isfinite(p.n.z) ||
            !std::isfinite(p.d) ||
            double(p.n.x)*p.n.x + double(p.n.y)*p.n.y + double(p.n.z)*p.n.z < 1e-12)
            return false;
    }
    return true;
}

// Use the existing local-light planes, including its intentionally omitted
// near plane. Prepare each equation once for all boxes in this shadow pass.
class PlaneSet
{
    struct Equation
    {
        double distance;
        double normal[3];
        double absNormal[3];
    };
    std::array<Equation, 12> equations;
    std::size_t size = 0;

    static bool OutsidePlane(const Bounds& bounds, const Equation& plane)
    {
        double distance = plane.distance, radius = 0;
        for (unsigned axis = 0; axis != 3; ++axis)
        {
            distance += plane.normal[axis] * bounds.center[axis];
            radius += plane.absNormal[axis] * bounds.extent[axis];
        }
        return distance > radius;
    }

public:
    template <class Plane>
    PlaneSet(const Plane* planes, std::size_t count)
    {
        if (!ValidPlanes(planes, count))
            return;
        for (std::size_t i = 0; i != count; ++i)
        {
            const auto& p = planes[i];
            const double x = p.n.x, y = p.n.y, z = p.n.z;
            equations[i] = {p.d, {x, y, z}, {std::fabs(x), std::fabs(y), std::fabs(z)}};
        }
        size = count;
    }

    bool Valid() const { return size != 0; }

    bool Outside(const Bounds& bounds) const
    {
        if (!bounds.valid)
            return false;
        for (std::size_t i = 0; i != size; ++i)
            if (OutsidePlane(bounds, equations[i]))
                return true;
        return false;
    }

    bool OutsideCoherent(const Bounds& bounds, std::size_t& lastRejectingPlane) const
    {
        if (!bounds.valid || !size)
            return false;
        if (lastRejectingPlane < size && OutsidePlane(bounds, equations[lastRejectingPlane]))
            return true;
        for (std::size_t i = 0; i != size; ++i)
        {
            if (i == lastRejectingPlane || !OutsidePlane(bounds, equations[i]))
                continue;
            lastRejectingPlane = i;
            return true;
        }
        return false;
    }
};

template <class Plane>
bool Outside(const Bounds& bounds, const Plane* planes, std::size_t count)
{
    return bounds.valid && PlaneSet(planes, count).Outside(bounds);
}
} // namespace xray::render::RENDER_NAMESPACE::GLDetailShadowCull
