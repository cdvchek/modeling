#pragma once

#include <types>
#include "core/math/vec3.hpp"
#include "ui/ui_context.hpp"
#include "scene/lights/light.hpp"

struct Headlight {
    bool enabled = true;
    Vec3 color { 1.0f, 1.0f, 1.0f };
    f32 strength = 0.4f;
};

struct ViewportSettings {
    Headlight headlight;
    bool showPanel = true;
    UIPanelState panel;
    LightType newLightType = LightType::Point;   // what the Lights tab's + button adds
    i32 newObjectPreset = 0;                     // index into objectPresets(); what the Objects tab's + button adds
};
