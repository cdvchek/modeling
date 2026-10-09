#pragma once

#include <memory>
#include <string>
#include <vector>
#include "core/math/vec3.hpp"
#include "scene/scene.hpp"

// What an object's faces can be painted into: one entry per texture its materials use as a base map, with the
// materials using it, then one per material without a map (painting it needs a new texture first)
struct PaintTarget {
    TextureHandle texture = INVALID_TEXTURE;    // INVALID_TEXTURE: materials[0] has no map yet
    std::vector<MaterialHandle> materials;
};

// Only materials some face draws with count: a face's own material, or the object's for faces without one (Default
// when that isn't valid). Entries are in the order their first face comes.
std::vector<PaintTarget> paintTargets(const Scene& scene, ObjectHandle object);

// The material the face draws with, resolved (Default when neither the face's nor the object's is valid)
MaterialHandle faceDrawMaterial(const Scene& scene, const Object& object, FaceHandle face);

// Whether paint into texture lands on the face: its material's base map is that texture
bool facePaints(const Scene& scene, const Object& object, FaceHandle face, TextureHandle texture);

// A width by height picture of one opaque sRGB color, as a PNG named fileName
std::shared_ptr<const Picture> solidPicture(const std::string& fileName, u32 width, u32 height, const Vec3& color);
