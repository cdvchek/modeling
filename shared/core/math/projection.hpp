#pragma once

#include <types>
#include "core/math/mat4.hpp"
#include "core/math/vec2.hpp"
#include "core/math/vec3.hpp"
#include "core/math/vec4.hpp"

// World point to pixel coordinates (origin top-left, y down); false if the point is behind the camera
inline bool projectToScreen(const Mat4& viewProjection, const Vec3& point, f32 width, f32 height, Vec2& out) {
    const Vec4 clip = viewProjection * Vec4(point.x, point.y, point.z, 1.0f);
    if (clip.w <= 1e-5f) return false;

    const f32 ndcX = clip.x / clip.w;
    const f32 ndcY = clip.y / clip.w;

    out = Vec2((ndcX * 0.5f + 0.5f) * width, (0.5f - ndcY * 0.5f) * height);
    return true;
}
