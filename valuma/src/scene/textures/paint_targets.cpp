#include "scene/textures/paint_targets.hpp"
#include "image/image.hpp"

#include <algorithm>
#include <cmath>

std::vector<PaintTarget> paintTargets(const Scene& scene, ObjectHandle handle) {
    std::vector<PaintTarget> targets;
    const Object* object = scene.objects.tryGet(handle);
    if (!object) return targets;

    std::vector<MaterialHandle> seen;
    std::vector<PaintTarget> unmapped;
    for (FaceHandle face : object->meshData.getFaceHandles()) {
        const MaterialHandle material = faceDrawMaterial(scene, *object, face);
        if (std::find(seen.begin(), seen.end(), material) != seen.end()) continue;
        seen.push_back(material);

        // A map that's gone reads as none
        const TextureHandle texture = scene.materials.get(material).baseColorMap;
        if (!scene.textures.isValid(texture)) {
            unmapped.push_back({ INVALID_TEXTURE, { material } });
            continue;
        }
        const auto existing = std::find_if(targets.begin(), targets.end(), [&](const PaintTarget& target) { return target.texture == texture; });
        if (existing != targets.end()) existing->materials.push_back(material);
        else targets.push_back({ texture, { material } });
    }
    targets.insert(targets.end(), unmapped.begin(), unmapped.end());
    return targets;
}

MaterialHandle faceDrawMaterial(const Scene& scene, const Object& object, FaceHandle face) {
    const MaterialHandle own = object.meshData.getFaceMaterial(face);
    return scene.materials.resolve(scene.materials.isValid(own) ? own : object.material);
}

bool facePaints(const Scene& scene, const Object& object, FaceHandle face, TextureHandle texture) {
    return scene.textures.isValid(texture) && scene.materials.get(faceDrawMaterial(scene, object, face)).baseColorMap == texture;
}

std::vector<FaceHandle> paintIsland(const Scene& scene, const Object& object, FaceHandle face, TextureHandle texture) {
    if (!facePaints(scene, object, face, texture)) return {};
    std::vector<FaceHandle> faces = object.meshData.getUVIsland(face);
    std::erase_if(faces, [&](FaceHandle other) { return !facePaints(scene, object, other, texture); });
    std::sort(faces.begin(), faces.end(), [](FaceHandle a, FaceHandle b) { return a.index < b.index; });
    return faces;
}

Vec2 texturePoint(Vec2 uv, u32 width, u32 height) {
    // 1 itself stays the far edge instead of wrapping to the near one
    const auto wrap = [](f32 value) { return value < 0.0f || value > 1.0f ? value - std::floor(value) : value; };
    return Vec2(wrap(uv.x) * static_cast<f32>(width), wrap(uv.y) * static_cast<f32>(height));
}

bool textureAxes(const Vec3 corners[3], const Vec2 uvs[3], u32 width, u32 height, Vec3& perX, Vec3& perY) {
    // Each edge is so much across and so much down on the texture; solving the two for one pixel of each
    const Vec3 edge1 = corners[1] - corners[0], edge2 = corners[2] - corners[0];
    const f32 x1 = (uvs[1].x - uvs[0].x) * static_cast<f32>(width), y1 = (uvs[1].y - uvs[0].y) * static_cast<f32>(height);
    const f32 x2 = (uvs[2].x - uvs[0].x) * static_cast<f32>(width), y2 = (uvs[2].y - uvs[0].y) * static_cast<f32>(height);
    const f32 area = x1 * y2 - x2 * y1;
    if (std::abs(area) < 1e-8f) return false;

    perX = (edge1 * y2 - edge2 * y1) * (1.0f / area);
    perY = (edge2 * x1 - edge1 * x2) * (1.0f / area);
    return true;
}

LayerStack solidLayers(u32 width, u32 height, const Vec3& color) {
    const auto channel = [](f32 value) { return static_cast<u8>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };

    LayerStack stack;
    stack.width = width;
    stack.height = height;
    fillLayer(stack, addLayer(stack, "Base"), { 0, 0, width, height }, { channel(color.x), channel(color.y), channel(color.z), 255 });
    return stack;
}

bool makeLayered(Texture& texture, std::string& error) {
    if (texture.layered()) return true;
    if (!texture.picture) {
        error = "it has no picture";
        return false;
    }
    if (!layersFromPicture(*texture.picture, texture.layers, error)) return false;

    texture.layers.layers[0].fromFile = true;
    texture.picture.reset();
    return true;
}
