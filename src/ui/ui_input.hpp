#pragma once

#include <types>
#include "core/math/vec2.hpp"

// Mouse state for one frame, filled by the application from its input system
struct UIInput {
    static constexpr u32 LEFT = 0;
    static constexpr u32 RIGHT = 1;
    static constexpr u32 MIDDLE = 2;
    static constexpr u32 BUTTON_COUNT = 3;

    Vec2 mouse;
    Vec2 mouseDelta;
    bool down[BUTTON_COUNT] = {};
    bool pressed[BUTTON_COUNT] = {};
    bool released[BUTTON_COUNT] = {};
    i32 scroll = 0;

    bool anyDown() const { return down[LEFT] || down[RIGHT] || down[MIDDLE]; }
    bool anyPressed() const { return pressed[LEFT] || pressed[RIGHT] || pressed[MIDDLE]; }
};
