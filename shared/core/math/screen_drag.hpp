#pragma once

#include <cmath>
#include <types>
#include "core/math/math_utils.hpp"
#include "core/math/vec2.hpp"

// Mouse angle around a screen point, counterclockwise as seen on screen (y down)
inline f32 screenAngle(Vec2 pivot, Vec2 point) {
    return std::atan2(pivot.y - point.y, point.x - pivot.x);
}

// Wraps an angle difference into [-pi, pi] so crossing the +-pi seam doesn't jump a full turn
inline f32 wrapAngle(f32 angle) {
    while (angle > Math::PI) angle -= Math::TWO_PI;
    while (angle < -Math::PI) angle += Math::TWO_PI;
    return angle;
}
