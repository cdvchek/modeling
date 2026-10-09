#pragma once

#include <types>
#include "core/math/vec2.hpp"
#include "scene/objects/origin.hpp"

#include <vector>

// Screen state for scale and rotate, filled on their first update
struct TransformTool {
    bool initialized = false;
    Vec2 pivot;                 // selection center on screen
    f32 startDistance = 0.0f;   // mouse distance from the pivot when the tool started
    f32 lastAngle = 0.0f;       // mouse angle around the pivot last frame
    f32 angle = 0.0f;           // total turn since the start, counterclockwise on screen
};

// What grab and rotate start from when they move an origin: where it was in the world, its mesh, and its children
struct OriginEdit {
    ObjectHandle object = INVALID_OBJECT;
    OriginStart start;
};
