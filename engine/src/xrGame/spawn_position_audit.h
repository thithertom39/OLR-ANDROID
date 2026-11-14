#pragma once
#include <cstdint>
#include <cstring>

// Used only on an already invalid spawn input. Raw bits distinguish denormal
// values rejected by _valid() from infinities and NaNs without FP arithmetic.
namespace spawn_position_audit
{
inline std::uint32_t Bits(const float& value) noexcept
{
    static_assert(sizeof(float) == sizeof(std::uint32_t), "binary32 diagnostic required");
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof result);
    return result;
}
inline const char* Classify(std::uint32_t bits) noexcept
{
    const auto magnitude = bits & 0x7fffffffu;
    if (magnitude == 0) return "zero";
    if (magnitude < 0x00800000u) return "subnormal";
    if (magnitude < 0x7f800000u) return "normal";
    if (magnitude == 0x7f800000u) return "infinite";
    return "nan";
}
inline const char* Text(const char* text) noexcept { return text ? text : "<null>"; }
template <class Vector> inline void VectorBits(const char* field, const Vector& value)
{
    const auto x = Bits(value.x), y = Bits(value.y), z = Bits(value.z);
    Msg("[spawn_pos_audit] field=%s value=(%.9g,%.9g,%.9g) bits=(%08x,%08x,%08x) class=(%s,%s,%s)",
        field, double(value.x), double(value.y), double(value.z),
        unsigned(x), unsigned(y), unsigned(z), Classify(x), Classify(y), Classify(z));
}
}
