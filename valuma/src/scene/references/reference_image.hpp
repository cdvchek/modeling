#pragma once

#include <memory>
#include <string>
#include <vector>
#include "core/containers/dynamic_array.hpp"
#include "core/math/vec3.hpp"
#include "scene/transform.hpp"
#include "scene/pictures/picture.hpp"

// Where an image draws against the rest of the scene
enum class ReferenceDepth : u8 {
    InScene,    // like any surface: hidden behind what's in front of it
    Behind,     // under everything, like a backdrop
    InFront     // over everything
};


// A picture on a plane in the scene, for modeling against. It keeps the picture's proportions: one size sets its
// height and the width follows. It faces its own +Z; its top is +Y.
struct ReferenceImage {
    std::string name;
    std::shared_ptr<const Picture> picture;

    Vec3 position { 0.0f };
    Vec3 rotation { 0.0f };     // Euler angles, applied like an object's
    f32 size = 2.0f;            // height in world units

    f32 opacity = 1.0f;
    ReferenceDepth depth = ReferenceDepth::InScene;
    bool locked = false;        // clicks in the viewport go through it
    bool visible = true;

    // Width over height of the picture
    f32 aspect() const;
    // Scaled to the plane's width and height, so it maps the unit square (-0.5 to 0.5 in X and Y) onto the plane
    Transform transform() const;
    Mat4 matrix() const { return transform().getMatrix(); }
};

using ReferenceHandle = Handle<ReferenceImage>;

constexpr ReferenceHandle INVALID_REFERENCE { INVALID_INDEX, 0 };

// Keeps sizes away from zero, where the plane would vanish and its matrix couldn't be inverted
inline constexpr f32 MIN_REFERENCE_SIZE = 0.001f;

const char* referenceDepthName(ReferenceDepth depth);
