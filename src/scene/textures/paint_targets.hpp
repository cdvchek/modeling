#pragma once

#include <memory>
#include <string>
#include <vector>
#include "core/math/vec2.hpp"
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

// The faces of the face's UV island that paint into texture, in handle order: where a dab on that face may spread
std::vector<FaceHandle> paintIsland(const Scene& scene, const Object& object, FaceHandle face, TextureHandle texture);

// A UV as a point on a texture, in its pixels from the top left; UVs outside 0 to 1 wrap around, as the texture repeats
Vec2 texturePoint(Vec2 uv, u32 width, u32 height);

// The step in the world for one texture pixel across (perX) and down (perY) on a triangle, from its world corners and their UVs; false if it has no area on the texture
bool textureAxes(const Vec3 corners[3], const Vec2 uvs[3], u32 width, u32 height, Vec3& perX, Vec3& perY);

// Layers for a new texture: a Base layer of one opaque sRGB color
LayerStack solidLayers(u32 width, u32 height, const Vec3& color);

// Gives a texture layers if it has none: its picture becomes the Base layer and is let go. False with error if the picture can't be read
bool makeLayered(Texture& texture, std::string& error);
