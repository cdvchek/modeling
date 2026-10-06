#pragma once

#include <types>
#include "core/math/vec2.hpp"

// Screen state for scale and rotate, filled on their first update
struct TransformTool {
    bool initialized = false;
    Vec2 pivot;                 // selection center on screen
    f32 startDistance = 0.0f;   // mouse distance from the pivot when the tool started
    f32 lastAngle = 0.0f;       // mouse angle around the pivot last frame
    f32 angle = 0.0f;           // total turn since the start, counterclockwise on screen
};
