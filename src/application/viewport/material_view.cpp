#include "application/viewport/material_view.hpp"
#include "application/paint/paint_workspace.hpp"
#include "application/app_context.hpp"
#include "scene/objects/origin.hpp"
#include "core/math/vec4.hpp"

SurfaceLook claySurface() {
    return SurfaceLook {};
}

namespace {
    bool hidden(const Material& material) {
        return (material.alphaMode == AlphaMode::Blend && material.opacity <= 0.0f)
            || (material.alphaMode == AlphaMode::Cutout && material.opacity < material.alphaCutoff);
    }

    // Faces that can't take paint: darker, and no glow
    constexpr f32 DIM = 0.3f;
    void dim(SurfaceLook& look) {
        look.baseColor = look.baseColor * DIM;
        look.emissiveStrength = 0.0f;
    }
}

bool materialView(const AppContext& ctx) {
    return ctx.viewport.showMaterials || ctx.workspace.current == Workspace::Paint;
}

u32 mapTexture(const AppContext& ctx, const Material& material) {
    const Texture* texture = ctx.scene.textures.tryGet(material.baseColorMap);
    return texture ? ctx.pictureTextures.find(texture->picture) : 0;
}

void syncTextures(AppContext& ctx) {
    for (TextureHandle handle : ctx.scene.textures.handles()) ctx.pictureTextures.sync(ctx, ctx.scene.textures.get(handle).picture);
}

SurfaceLook surfaceOf(const Material& material, u32 map) {
    SurfaceLook look;
    look.baseColorMap = map;
    look.baseColor = material.baseColor;
    look.roughness = material.roughness;
    look.metallic = material.metallic;
    look.emissiveColor = material.emissiveColor;
    look.emissiveStrength = material.emissiveStrength;
    // A map's alpha can make any pixel see-through, so a blended material with one always blends
    look.blend = material.alphaMode == AlphaMode::Blend && (material.opacity < 1.0f || map != 0);
    look.opacity = material.opacity;
    if (material.alphaMode == AlphaMode::Cutout) look.alphaCutoff = material.alphaCutoff;
    look.backFaces = material.doubleSided ? BackFaces::Lit : BackFaces::Culled;

    if (hidden(material)) {
        look.blend = true;
        look.opacity = 0.0f;
    }
    return look;
}

std::optional<SurfaceLook> surfaceFor(const AppContext& ctx, const Object& object) {
    if (!materialView(ctx)) return claySurface();

    const MaterialCollection& materials = ctx.scene.materials;
    const Material& material = materials.get(materials.resolve(object.material));
    if (hidden(material)) return std::nullopt;
    return surfaceOf(material, mapTexture(ctx, material));
}

void MaterialPreviewCache::sync(AppContext& ctx) {
    const MaterialCollection& materials = ctx.scene.materials;

    // Removed materials free their textures (undoing the removal makes a new one)
    std::erase_if(m_entries, [&](const Entry& entry) {
        if (materials.isValid(entry.handle)) return false;
        ctx.renderer->destroyTexture(entry.texture);
        return true;
    });

    for (MaterialHandle handle : materials.handles()) {
        const SurfaceLook look = surfaceOf(materials.get(handle), mapTexture(ctx, materials.get(handle)));

        Entry* entry = nullptr;
        for (Entry& existing : m_entries) {
            if (existing.handle == handle) entry = &existing;
        }

        if (!entry) {
            m_entries.push_back({ handle, look, ctx.renderer->renderMaterialPreview(look, SIZE, 0) });
        } else if (!sameSurface(entry->look, look)) {
            entry->look = look;
            entry->texture = ctx.renderer->renderMaterialPreview(look, SIZE, entry->texture);
        }
    }
}

u32 MaterialPreviewCache::texture(MaterialHandle handle) const {
    for (const Entry& entry : m_entries) {
        if (entry.handle == handle) return entry.texture;
    }
    return 0;
}

ObjectParts partsFor(const AppContext& ctx, const Object& object, const IMesh& mesh) {
    ObjectParts parts;
    if (!materialView(ctx)) {
        parts.whole = true;
        return parts;
    }

    // Paint: faces whose material's map isn't the texture being painted are dimmed
    const bool painting = ctx.workspace.current == Workspace::Paint;
    const TextureHandle painted = painting ? activePaintTexture(ctx) : INVALID_TEXTURE;

    // A group's material, or the object's for faces without their own; a material that shows nothing isn't drawn
    const MaterialCollection& materials = ctx.scene.materials;
    for (const FaceGroup& group : mesh.faceGroups()) {
        const MaterialHandle handle = materials.resolve(materials.isValid(group.material) ? group.material : object.material);
        const Material& material = materials.get(handle);
        if (hidden(material)) continue;

        DrawPart part { group.firstIndex, group.indexCount, surfaceOf(material, mapTexture(ctx, material)) };
        if (painting && (material.baseColorMap != painted || !ctx.scene.textures.isValid(painted))) dim(part.surface);
        if (part.surface.blend) parts.seeThrough.push_back(part);
        else parts.solid.push_back(part);
    }
    return parts;
}

FaceGroupOf faceGroupsFor(const AppContext& ctx) {
    const MaterialCollection* materials = &ctx.scene.materials;
    return [materials](MaterialHandle material) { return materials->isValid(material) ? material : INVALID_MATERIAL; };
}

bool backFacesCulled(const AppContext& ctx, ObjectHandle handle) {
    const Object* object = ctx.scene.objects.tryGet(handle);
    if (!materialView(ctx) || !object) return false;
    return !ctx.scene.materials.get(ctx.scene.materials.resolve(object->material)).doubleSided;
}

Vec3 worldCenter(const AppContext& ctx, ObjectHandle handle) {
    Vec3 low, high;
    localBounds(ctx.scene.objects.get(handle).meshData, low, high);
    const Vec3 center = (low + high) * 0.5f;
    const Vec4 world = ctx.scene.objects.worldMatrix(handle) * Vec4(center.x, center.y, center.z, 1.0f);
    return Vec3(world.x, world.y, world.z);
}
