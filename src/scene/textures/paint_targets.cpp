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

std::shared_ptr<const Picture> solidPicture(const std::string& fileName, u32 width, u32 height, const Vec3& color) {
    const auto channel = [](f32 value) { return static_cast<u8>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
    const u8 pixel[4] = { channel(color.x), channel(color.y), channel(color.z), 255 };

    image::Image image;
    image.width = width;
    image.height = height;
    image.pixels.resize(std::size_t(width) * height * 4);
    for (std::size_t i = 0; i < image.pixels.size(); i += 4) std::copy(pixel, pixel + 4, image.pixels.begin() + i);

    auto picture = std::make_shared<Picture>();
    picture->fileName = fileName;
    picture->png = image::encodePng(image);
    picture->width = width;
    picture->height = height;
    return picture;
}
