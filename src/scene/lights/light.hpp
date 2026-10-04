#pragma once

#include <string>
#include "core/math/vec3.hpp"
#include "core/containers/dynamic_array.hpp"

enum class LightType {
    Point,
    Directional,
    Spot
};

struct Light {
    std::string name;
    LightType type = LightType::Point;

    Vec3 position { 0.0f, 3.0f, 0.0f };
    Vec3 direction { 0.0f, -1.0f, 0.0f };

    Vec3 color { 1.0f, 1.0f, 1.0f };
    f32 intensity = 1.0f;

    // Point and spot only
    f32 range = 10.0f;

    // Spot only, half-angles in radians
    f32 innerConeRadians = 0.35f;
    f32 outerConeRadians = 0.5f;

    bool enabled = true;
};

struct AmbientLight {
    Vec3 color { 1.0f, 1.0f, 1.0f };
    f32 strength = 0.6f;
};

using LightHandle = Handle<Light>;

constexpr LightHandle INVALID_LIGHT { INVALID_INDEX, 0 };
