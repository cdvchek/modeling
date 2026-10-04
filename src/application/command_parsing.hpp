#pragma once

#include <string>
#include <cstdlib>
#include <cmath>
#include <types>
#include "core/math/vec3.hpp"
#include "core/console/command_system.hpp"

inline bool parseFloat(const std::string& text, f32& out) {
    char* end = nullptr;
    const f32 value = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || end != text.c_str() + text.size()) return false;
    if (!std::isfinite(value)) return false;

    out = value;
    return true;
}

inline bool parseUnitFloat(const std::string& text, f32& out) {
    f32 value = 0.0f;
    if (!parseFloat(text, value) || value < 0.0f || value > 1.0f) return false;

    out = value;
    return true;
}

inline bool parseU32(const std::string& text, u32& out) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text.c_str(), &end, 10);
    if (text.empty() || text[0] == '-' || end != text.c_str() + text.size()) return false;

    out = static_cast<u32>(value);
    return true;
}

// Reads args[first], args[first + 1], args[first + 2]; the args must end there
inline bool parseVec3Args(const CommandArgs& args, std::size_t first, Vec3& out, bool unitRange) {
    if (args.size() != first + 3) return false;

    bool (*parse)(const std::string&, f32&) = unitRange ? parseUnitFloat : parseFloat;
    return parse(args[first], out.x) && parse(args[first + 1], out.y) && parse(args[first + 2], out.z);
}
