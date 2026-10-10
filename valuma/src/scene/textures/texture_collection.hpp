#pragma once

#include <string>
#include <vector>
#include "scene/textures/texture.hpp"

// The project's textures. A material's texture handle that isn't valid (none set, or its texture was removed) means
// no map, so removing a texture puts its materials back on their plain colors without touching them.
class TextureCollection {
public:
    TextureHandle add(Texture texture);
    void remove(TextureHandle handle);

    bool isValid(TextureHandle handle) const;

    Texture& get(TextureHandle handle);
    const Texture& get(TextureHandle handle) const;
    Texture* tryGet(TextureHandle handle);
    const Texture* tryGet(TextureHandle handle) const;

    // In slot order
    std::vector<TextureHandle> handles() const;
    TextureHandle handleAt(u32 slot) const;
    u32 count() const;

    // base, or "base N" with the lowest N that isn't taken
    std::string uniqueName(const std::string& base) const;

    // A texture whose PNG is byte for byte the same, if there is one (import reuses it)
    TextureHandle findSamePicture(const Picture& picture) const;

private:
    DynamicArray<Texture> m_textures;
};
