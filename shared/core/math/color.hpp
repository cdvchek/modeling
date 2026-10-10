#pragma once

#include <cmath>
#include "core/math/vec3.hpp"

// Colors are picked and stored as sRGB (what color pickers, image editors, and engines show); lighting is done on
// linear values, so a picked color is converted before it's lit and the result converted back for the screen.

inline f32 srgbToLinear(f32 value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

inline f32 linearToSrgb(f32 value) {
    return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

inline Vec3 srgbToLinear(const Vec3& color) {
    return Vec3(srgbToLinear(color.x), srgbToLinear(color.y), srgbToLinear(color.z));
}

inline Vec3 linearToSrgb(const Vec3& color) {
    return Vec3(linearToSrgb(color.x), linearToSrgb(color.y), linearToSrgb(color.z));
}
