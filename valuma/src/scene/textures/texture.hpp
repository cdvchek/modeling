#pragma once

#include <memory>
#include <string>
#include "core/containers/dynamic_array.hpp"
#include "scene/pictures/picture.hpp"
#include "scene/textures/layers.hpp"

// A picture materials can use as a map, shared by every material that points at it (so several objects can share one
// texture, as an atlas). Its pixels are a Picture as it was loaded, or layers once it's painted; never both.
struct Texture {
    std::string name;
    // The PNG file as loaded, shared by copies (undo); null once the texture has layers
    std::shared_ptr<const Picture> picture;
    // The file it was loaded from (UTF-8), so Reload can read it again; empty when unknown (an imported asset)
    std::string sourcePath;
    // Its paintable pixels, once it has been painted or given layers; empty until then. Copies share the tiles
    LayerStack layers;

    bool layered() const { return !layers.empty(); }
    u32 width() const { return layered() ? layers.width : picture ? picture->width : 0; }
    u32 height() const { return layered() ? layers.height : picture ? picture->height : 0; }
};

using TextureHandle = Handle<Texture>;

constexpr TextureHandle INVALID_TEXTURE { INVALID_INDEX, 0 };
