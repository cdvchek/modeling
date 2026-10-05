#pragma once

#include <types>
#include "core/math/vec3.hpp"
#include "ui/ui_context.hpp"

struct Headlight {
    bool enabled = true;
    Vec3 color { 1.0f, 1.0f, 1.0f };
    f32 strength = 0.4f;
};

struct ViewportSettings {
    Headlight headlight;
    bool showPanel = true;
    UIPanelState panel;
};
