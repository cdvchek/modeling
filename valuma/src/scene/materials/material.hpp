#pragma once

#include <string>
#include "core/containers/dynamic_array.hpp"
#include "core/math/vec3.hpp"
#include "scene/textures/texture.hpp"

// How a material's opacity is used, as in glTF and the game engines
enum class AlphaMode : u8 {
    Opaque,     // opacity is ignored
    Cutout,     // drawn where opacity reaches the cutoff, not at all below it
    Blend       // see-through by opacity, drawn after everything solid, farthest first
};

// How a surface looks: the metallic-roughness set glTF and the engines use, so it exports as is.
// Colors are sRGB, as picked.
struct Material {
    std::string name;

    Vec3 baseColor { 0.72f, 0.73f, 0.78f };
    // A picture multiplied by baseColor (its alpha by opacity), read through the mesh's UVs; not valid means none
    TextureHandle baseColorMap = INVALID_TEXTURE;
    f32 roughness = 0.5f;       // 0 mirror-sharp highlights, 1 none
    f32 metallic = 0.0f;        // 0 plastic, stone, wood; 1 metal

    Vec3 emissiveColor { 1.0f, 1.0f, 1.0f };
    f32 emissiveStrength = 0.0f;    // 0 gives no glow

    f32 opacity = 1.0f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    f32 alphaCutoff = 0.5f;

    // Back faces are drawn (lit as if they faced you); otherwise they're culled, as the engine will
    bool doubleSided = false;

    // Every value equal (the name aside)
    bool sameLook(const Material& other) const;
};

using MaterialHandle = Handle<Material>;

constexpr MaterialHandle INVALID_MATERIAL { INVALID_INDEX, 0 };

const char* alphaModeName(AlphaMode mode);
