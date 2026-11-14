#pragma once

#include "glUniformBatch.h"
#include <array>

namespace xray::render::RENDER_NAMESPACE::GLDetailUniformBatch
{
// The shipped GL detail shaders declare array[61 * 4]. Unexpected batch sizes
// or constant layouts keep the existing per-element upload path.
constexpr u32 RowsPerInstance = 4;
constexpr u32 MaxInstances = 61;

inline bool CanBatch(const R_constant& constant, size_t batchSize)
{
    return batchSize > 0 && batchSize <= MaxInstances && constant.type == RC_float &&
        constant.destination == RC_dest_all && constant.pp.cls == RC_1x4 &&
        constant.pp.program && constant.pp.location != GLuint(-1);
}

inline void Pack(const Fmatrix& matrix, float scale, float sun, float hemi, Fvector4* rows)
{
    // Preserve the exact expressions and row/component order of the four
    // original set_ca calls, including unscaled translation and color.
    rows[0].set(matrix._11 * scale, matrix._21 * scale, matrix._31 * scale, matrix._41);
    rows[1].set(matrix._12 * scale, matrix._22 * scale, matrix._32 * scale, matrix._42);
    rows[2].set(matrix._13 * scale, matrix._23 * scale, matrix._33 * scale, matrix._43);
    rows[3].set(sun, sun, sun, hemi);
}
} // namespace xray::render::RENDER_NAMESPACE::GLDetailUniformBatch
