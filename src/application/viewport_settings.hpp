#pragma once

#include <types>
#include "core/math/vec3.hpp"
#include "ui/ui_context.hpp"
#include "scene/lights/light.hpp"
#include "scene/objects/object_collection.hpp"

#include <vector>

struct Headlight {
    bool enabled = true;
    Vec3 color { 1.0f, 1.0f, 1.0f };
    f32 strength = 0.4f;
};

struct ViewportSettings {
    Headlight headlight;
    bool showPanel = true;
    bool showOrigins = true;   // origin markers in the viewport (View menu)
    UIPanelState panel;
    LightType newLightType = LightType::Point;   // what the Lights tab's + button adds
    std::vector<ObjectHandle> foldedObjects;     // objects whose children the Objects tab hides
    i32 newObjectPreset = 0;                     // index into objectPresets() (one past the end: Import...); what the Objects tab's + button adds
};
