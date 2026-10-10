#pragma once

#include <types>

#include "core/math/vec3.hpp"
#include "core/math/mat4.hpp"

class Camera {
public:
    Camera();

    Vec3 position;
    Vec3 target;
    Vec3 up;

    f32 distance;
    f32 yaw = 0;
    f32 pitch = 0;

    f32 fovRadians;
    f32 nearPlane;
    f32 farPlane;

    void updatePositionFromOrbit();

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix(f32 aspectRatio) const;

    Vec3 getForward() const;
    Vec3 getRight() const;
};