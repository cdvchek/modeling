#include "core/scene/camera.hpp"

Camera::Camera()
    : position(0.0f, 0.0f, 3.0f),
      target(0.0f, 0.0f, 0.0f),
      up(0.0f, 1.0f, 0.0f),
      fovRadians(1.04719755f), // 60 degrees
      nearPlane(0.1f),
      farPlane(100.0f) {}

Mat4 Camera::getViewMatrix() const {
    return Mat4::lookAt(
        position,
        target,
        up
    );
}

Mat4 Camera::getProjectionMatrix(f32 aspectRatio) const {
    return Mat4::perspective(
        fovRadians,
        aspectRatio,
        nearPlane,
        farPlane
    );
}