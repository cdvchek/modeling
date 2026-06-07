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