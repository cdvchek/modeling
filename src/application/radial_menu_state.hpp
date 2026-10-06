#pragma once

#include <types>
#include "core/math/vec2.hpp"

enum class RadialMenuId : u8 {
    None,
    Main,
    Edit,
    Light,
    Mode,
    View,
    Tool
};

struct RadialMenuState {
    bool open = false;
    RadialMenuId menu = RadialMenuId::Main;
    Vec2 center;
    i32 hovered = -1;
};
