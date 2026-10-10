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

// The Euler angles (as getMatrix applies them) that turn the X, Y, and Z axes onto x, y, and z, which must be
// unit length, at right angles, and right-handed
Vec3 eulerFromAxes(const Vec3& x, const Vec3& y, const Vec3& z);

// A child's world transform from its parent's world transform and its own (relative) one, without skew:
// the position goes where it belongs in the parent (scaled, turned, moved by it), the rotations combine, and the
// parent's scale multiplies the child's along the child's own axes. Unreal Engine combines transforms the same way.
Transform combineTransforms(const Transform& parent, const Transform& local);

// The reverse: the relative transform that puts a child at world under parent, exactly (no skew to lose)
Transform relativeTransform(const Transform& parent, const Transform& world);