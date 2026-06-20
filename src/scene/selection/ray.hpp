#pragma once

#include <types>

#include "core/math/vec3.hpp"

class Camera;

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

Ray makeRayFromScreenPosition(
    i32 mouseX,
    i32 mouseY,
    u32 width,
    u32 height,
    const Camera& camera
);