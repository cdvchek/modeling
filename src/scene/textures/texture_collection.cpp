#include "scene/textures/texture_collection.hpp"

TextureHandle TextureCollection::add(Texture texture) {
    return m_textures.insert(std::move(texture));
}

void TextureCollection::remove(TextureHandle handle) {
    m_textures.remove(handle);
}

bool TextureCollection::isValid(TextureHandle handle) const {
    return m_textures.isValid(handle);
}

Texture& TextureCollection::get(TextureHandle handle) {
    return m_textures.get(handle);
}

const Texture& TextureCollection::get(TextureHandle handle) const {
    return m_textures.get(handle);
}

Texture* TextureCollection::tryGet(TextureHandle handle) {
    return m_textures.tryGet(handle);
}

const Texture* TextureCollection::tryGet(TextureHandle handle) const {
    return m_textures.tryGet(handle);
}

std::vector<TextureHandle> TextureCollection::handles() const {
    return m_textures.getActiveHandles();
}

TextureHandle TextureCollection::handleAt(u32 slot) const {
    if (slot >= m_textures.size()) return INVALID_TEXTURE;
    return m_textures.getHandle(slot);
}

u32 TextureCollection::count() const {
    return m_textures.activeSize();
}

std::string TextureCollection::uniqueName(const std::string& base) const {
    const auto taken = [this](const std::string& name) {
        for (TextureHandle handle : handles()) {
            if (m_textures.get(handle).name == name) return true;
        }
        return false;
    };

    if (!taken(base)) return base;

    for (u32 number = 2;; ++number) {
        const std::string name = base + " " + std::to_string(number);
        if (!taken(name)) return name;
    }
}

TextureHandle TextureCollection::findSamePicture(const Picture& picture) const {
    for (TextureHandle handle : handles()) {
        const Texture& texture = m_textures.get(handle);
        if (texture.picture && texture.picture->png == picture.png) return handle;
    }
    return INVALID_TEXTURE;
}
