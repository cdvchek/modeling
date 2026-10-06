#pragma once

#include <types>
#include "core/math/vec2.hpp"

// Slice math for radial menus: count equal slices, slice 0 centered straight up, the rest clockwise on screen (y down)
namespace RadialLayout {
    // Counterclockwise from right, in radians, as UIDrawList::ringSlice takes it
    f32 sliceAngle(u32 slice, u32 count);
    Vec2 sliceDirection(u32 slice, u32 count);

    // The slice an offset from the menu center points into, or -1 inside the dead zone
    i32 sliceAt(Vec2 offset, f32 deadZone, u32 count);
}
