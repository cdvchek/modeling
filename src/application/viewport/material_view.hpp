#pragma once

#include <optional>
#include <vector>
#include "renderer/renderer.hpp"
#include "scene/objects/object_collection.hpp"
#include "scene/materials/material_collection.hpp"

struct AppContext;

// A material's own look, whatever the view: back faces culled unless double-sided. A material that shows nothing
// (blended at opacity 0, a cutout below its cutoff) comes back blended at opacity 0. map is its base color map's
// texture (mapTexture), 0 for none.
SurfaceLook surfaceOf(const Material& material, u32 map);

// The GPU texture of the material's base color map, 0 when it has none or it isn't uploaded yet
u32 mapTexture(const AppContext& ctx, const Material& material);

// Uploads every texture's picture that isn't on the GPU yet; call before anything draws with maps or swatches
void syncTextures(AppContext& ctx);

// Swatch textures, one per material, rendered again whenever the material's look changes
class MaterialPreviewCache {
public:
    static constexpr u32 SIZE = 128;

    // Renders what changed and frees what was removed; call before the main pass
    void sync(AppContext& ctx);
    // 0 until the material's first sync
    u32 texture(MaterialHandle handle) const;

private:
    struct Entry {
        MaterialHandle handle;
        SurfaceLook look;
        u32 texture = 0;
    };

    std::vector<Entry> m_entries;
};

// The plain look of clay view (View > Materials off): the Default gray, back faces tinted
SurfaceLook claySurface();

// Materials show: View > Materials is on, or the Paint workspace (always)
bool materialView(const AppContext& ctx);

// How an object's faces draw: its material's look in material view, clay otherwise. Empty when nothing would show,
// so it isn't drawn at all: blended at opacity 0, or a cutout whose opacity is below its cutoff (a map can only
// lower it further). Blend at full opacity draws as opaque, which is the same picture for less work.
std::optional<SurfaceLook> surfaceFor(const AppContext& ctx, const Object& object);

// Back faces aren't drawn: material view with a single-sided material. Clicks then skip them too.
bool backFacesCulled(const AppContext& ctx, ObjectHandle handle);

// An object's faces split by material for drawing: solid parts draw with the object, see-through ones are sorted
// with the rest. In clay view there are no parts: every face draws at once in claySurface(). In Paint, faces whose
// material's base map isn't the texture being painted draw dimmed.
struct ObjectParts {
    bool whole = false;
    std::vector<DrawPart> solid;
    std::vector<DrawPart> seeThrough;
};
ObjectParts partsFor(const AppContext& ctx, const Object& object, const IMesh& mesh);

// Which draw group a face's own material puts it in: itself while it exists, otherwise the object's (INVALID)
FaceGroupOf faceGroupsFor(const AppContext& ctx);

// The middle of the object's bounding box in the world, for sorting see-through objects
Vec3 worldCenter(const AppContext& ctx, ObjectHandle handle);
