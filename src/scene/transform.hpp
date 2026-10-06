#pragma once

#include "core/math/vec3.hpp"
#include "core/math/mat4.hpp"

struct Transform {
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;

    Transform();

    Mat4 getMatrix() const;
};

// Converts between an object's mesh space and world space, so tools can work in world space on any transform
struct ObjectSpace {
    Mat4 toWorld;
    Mat4 toLocal;

    explicit ObjectSpace(const Transform& transform);

    Vec3 pointToWorld(const Vec3& point) const;
    Vec3 pointToLocal(const Vec3& point) const;
    // Directions ignore the position
    Vec3 directionToLocal(const Vec3& direction) const;
};

// Euler angles (applied X, then Y, then Z, as getMatrix does) after a further world-space turn of angle around axis
Vec3 rotateEuler(const Vec3& euler, const Vec3& axis, f32 angle);