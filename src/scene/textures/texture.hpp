#pragma once

#include <memory>
#include <string>
#include "core/containers/dynamic_array.hpp"
#include "scene/pictures/picture.hpp"

// A picture materials can use as a map, shared by every material that points at it (so several objects can share one
// texture, as an atlas). Its pixels are a Picture, which copies of the texture (undo) share.
struct Texture {
    std::string name;
    std::shared_ptr<const Picture> picture;
    // The file it was loaded from (UTF-8), so Reload can read it again; empty when unknown (an imported asset)
    std::string sourcePath;
};

using TextureHandle = Handle<Texture>;

constexpr TextureHandle INVALID_TEXTURE { INVALID_INDEX, 0 };
