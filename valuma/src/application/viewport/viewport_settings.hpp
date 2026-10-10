#pragma once

#include "scene/materials/material.hpp"

#include <types>
#include "core/math/vec3.hpp"
#include "ui/ui_context.hpp"
#include "scene/lights/light.hpp"
#include "scene/objects/object_collection.hpp"

#include <vector>

struct Headlight {
    bool enabled = true;
    Vec3 color { 1.0f, 1.0f, 1.0f };
    f32 strength = 0.8f;
};

struct ViewportSettings {
    Headlight headlight;
    // The material the Materials tab shows; not valid means Default
    MaterialHandle selectedMaterial = INVALID_MATERIAL;
    // The texture the Materials tab's Textures section shows; not valid means none
    TextureHandle selectedTexture = INVALID_TEXTURE;

    // The stats readout in the top left (stats command); not saved
    bool showStats = false;

    // Materials as they'll look in the game; off is clay view: everything the Default gray, back faces tinted
    bool showMaterials = true;
    // A colored grid over every face in place of its base color, to see the UVs
    bool showUVChecker = false;
    // Stops: each +1 doubles how bright lit surfaces look, before tone mapping
    f32 exposure = 0.0f;
    static constexpr f32 MIN_EXPOSURE = -5.0f;
    static constexpr f32 MAX_EXPOSURE = 5.0f;
    bool showPanel = true;
    bool showOrigins = true;   // origin markers in the viewport (View menu)
    UIPanelState panel;
    LightType newLightType = LightType::Point;   // what the Lights tab's + button adds
    std::vector<ObjectHandle> foldedObjects;     // objects whose children the Objects tab hides
    i32 newObjectPreset = 0;                     // index into objectPresets() (one past the end: Import...); what the Objects tab's + button adds
};
