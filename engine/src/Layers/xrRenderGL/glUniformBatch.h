#pragma once

#include <cstring>

namespace xray::render::RENDER_NAMESPACE::GLUniformBatch
{
// skin.h declares sbones_array[256 - 22], with three vec4 rows per bone.
// Larger or unexpected layouts retain the existing per-element upload path.
constexpr u32 MaxBones = (256 - 22) / 3;

inline bool Enabled()
{
    static const bool enabled = []
    {
        if (!Core.Params)
            return false;
        constexpr char flag[] = "-gles_uniform_batch";
        constexpr size_t length = sizeof(flag) - 1;
        for (pcstr arg = Core.Params; (arg = std::strstr(arg, flag)) != nullptr; ++arg)
        {
            if ((arg == Core.Params || std::strchr(" \t\r\n", arg[-1])) &&
                (arg[length] == '\0' || std::strchr(" \t\r\n", arg[length])))
                return true;
        }
        return false;
    }();
    return enabled;
}

inline bool CanBatch(const R_constant& constant, u32 bones)
{
    return bones > 0 && bones <= MaxBones && constant.type == RC_float &&
        constant.destination == RC_dest_all && constant.pp.cls == RC_1x4 &&
        constant.pp.program && constant.pp.location != GLuint(-1);
}

inline void Pack(const Fmatrix& matrix, Fvector4* rows)
{
    // This is the same component order as the three original set_ca calls.
    rows[0].set(matrix._11, matrix._21, matrix._31, matrix._41);
    rows[1].set(matrix._12, matrix._22, matrix._32, matrix._42);
    rows[2].set(matrix._13, matrix._23, matrix._33, matrix._43);
}
} // namespace xray::render::RENDER_NAMESPACE::GLUniformBatch
