#include "core/scene/transform.hpp"

Transform::Transform() : position(0.0f), rotation(0.0f), scale(1.0f) {}

Mat4 Transform::getMatrix() const {
    Mat4 translation = Mat4::translation(position);
    Mat4 rotationX = Mat4::rotationX(rotation.x);
    Mat4 rotationY = Mat4::rotationY(rotation.y);
    Mat4 rotationZ = Mat4::rotationZ(rotation.z);
    Mat4 scaling = Mat4::scale(scale);

    return translation * rotationZ * rotationY * rotationX * scaling;
}