#include "core/scene/camera.hpp"

Camera::Camera()
    : position(0.0f, 0.0f, 3.0f),
      target(0.0f, 0.0f, 0.0f),
      up(0.0f, 1.0f, 0.0f),
      distance(3.0f),
      fovRadians(1.04719755f), // 60 degrees
      nearPlane(0.1f),
      farPlane(100.0f) {}

void Camera::updatePositionFromOrbit() {
    f32 cosPitch = std::cos(pitch);
    position.x = target.x + distance * cosPitch * std::sin(yaw);
    position.y = target.y + distance * std::sin(pitch);
    position.z = target.z + distance * cosPitch * std::cos(yaw);
}

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

Vec3 Camera::getForward() const {
    return (target - position).normalized();
}

Vec3 Camera::getRight() const {
    return Vec3::cross(getForward(), up).normalized();
}