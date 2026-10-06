#pragma once

#include <types>

#include "core/math/vec3.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "scene/selection/selection.hpp"

// Bevel and inset: new vertices slide by a width that follows the mouse
struct WidthTool {
    ObjectHandle object = INVALID_OBJECT;
    SlideSession session;

    Vec3 pivot;
    // Mouse position when the tool started; the width follows how far the mouse has moved from it
    f32 startMouseX = 0.0f;
    f32 startMouseY = 0.0f;

    Selection savedSelection;
};
