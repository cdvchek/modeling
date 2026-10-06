#include "ui/radial_layout.hpp"
#include "core/math/math_utils.hpp"

#include <cmath>

f32 RadialLayout::sliceAngle(u32 slice, u32 count) {
    return Math::HALF_PI - Math::TWO_PI * static_cast<f32>(slice) / static_cast<f32>(count);
}

Vec2 RadialLayout::sliceDirection(u32 slice, u32 count) {
    const f32 angle = sliceAngle(slice, count);
    return Vec2(std::cos(angle), -std::sin(angle));
}

i32 RadialLayout::sliceAt(Vec2 offset, f32 deadZone, u32 count) {
    if (count == 0 || offset.length() < deadZone) return -1;

    // Clockwise from straight up
    f32 angle = std::atan2(offset.x, -offset.y);
    if (angle < 0.0f) angle += Math::TWO_PI;

    const f32 step = Math::TWO_PI / static_cast<f32>(count);
    return static_cast<i32>(std::lround(angle / step)) % static_cast<i32>(count);
}
