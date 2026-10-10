#include "application/viewport/pictures.hpp"
#include "application/app_context.hpp"
#include "image/image.hpp"

#include <algorithm>
#include <fstream>

namespace {
    bool samePicture(const std::weak_ptr<const Picture>& a, const std::shared_ptr<const Picture>& b) {
        return !a.owner_before(b) && !b.owner_before(a);
    }
}

std::string utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

u32 PictureTextureCache::sync(AppContext& ctx, const std::shared_ptr<const Picture>& picture) {
    if (!picture) return 0;

    for (const Entry& entry : m_entries) {
        if (samePicture(entry.picture, picture)) return entry.texture;
    }

    // A failed picture keeps an entry with no texture, so it's reported and tried once
    Entry entry;
    entry.picture = picture;

    image::Image decoded;
    std::string error;
    if (!image::decodePng(picture->png.data(), picture->png.size(), decoded, error)) {
        ctx.systems.console.printError("Couldn't show " + picture->fileName + ": " + error);
    } else {
        entry.texture = ctx.renderer->createTexture(decoded.pixels.data(), decoded.width, decoded.height);
        if (entry.texture == 0) ctx.systems.console.printError("Couldn't show " + picture->fileName + ": it's too large for the graphics card");
    }

    m_entries.push_back(entry);
    return entry.texture;
}

u32 PictureTextureCache::find(const std::shared_ptr<const Picture>& picture) const {
    if (!picture) return 0;
    for (const Entry& entry : m_entries) {
        if (samePicture(entry.picture, picture)) return entry.texture;
    }
    return 0;
}

void PictureTextureCache::prune(IRenderer& renderer) {
    std::erase_if(m_entries, [&](const Entry& entry) {
        if (!entry.picture.expired()) return false;
        renderer.destroyTexture(entry.texture);
        return true;
    });
}

void LayerTextureCache::sync(AppContext& ctx) {
    const TextureCollection& textures = ctx.scene.textures;
    IRenderer& renderer = *ctx.renderer;

    // Textures that are gone or no longer layered (undo) free theirs
    std::erase_if(m_entries, [&](const Entry& entry) {
        const Texture* texture = textures.tryGet(entry.handle);
        if (texture && !texture->layers.empty()) return false;
        renderer.destroyTexture(entry.texture);
        return true;
    });

    for (TextureHandle handle : textures.handles()) {
        const LayerStack& layers = textures.get(handle).layers;
        if (layers.empty()) continue;

        auto entry = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& existing) { return existing.handle == handle; });
        if (entry == m_entries.end()) entry = m_entries.insert(m_entries.end(), Entry { handle });
        entry->changed = false;

        // A new texture, or another size: the whole picture
        if (entry->shown.width != layers.width || entry->shown.height != layers.height) {
            renderer.destroyTexture(entry->texture);
            m_pixels.resize(std::size_t(layers.width) * layers.height * 4);
            compositeLayers(layers, { 0, 0, layers.width, layers.height }, m_pixels.data());
            entry->texture = renderer.createTexture(m_pixels.data(), layers.width, layers.height);
            if (entry->texture == 0) ctx.systems.console.printError("Couldn't show " + textures.get(handle).name + ": it's too large for the graphics card");
            entry->shown = layers;
            entry->changed = true;
            continue;
        }
        // One that couldn't be made stays reported once
        if (entry->texture == 0) continue;

        const std::vector<PixelRect> rects = changedRects(entry->shown, layers);
        for (const PixelRect& rect : rects) {
            m_pixels.resize(std::size_t(rect.width) * rect.height * 4);
            compositeLayers(layers, rect, m_pixels.data());
            renderer.updateTexture(entry->texture, rect.x, rect.y, rect.width, rect.height, m_pixels.data());
        }
        if (!rects.empty()) renderer.refreshTextureMipmaps(entry->texture);
        entry->shown = layers;
        entry->changed = !rects.empty();
    }
}

u32 LayerTextureCache::find(TextureHandle handle) const {
    for (const Entry& entry : m_entries) {
        if (entry.handle == handle) return entry.texture;
    }
    return 0;
}

bool LayerTextureCache::changed(TextureHandle handle) const {
    for (const Entry& entry : m_entries) {
        if (entry.handle == handle) return entry.changed;
    }
    return false;
}

u32 textureImage(const AppContext& ctx, TextureHandle handle) {
    const Texture* texture = ctx.scene.textures.tryGet(handle);
    if (!texture) return 0;
    return texture->layers.empty() ? ctx.pictureTextures.find(texture->picture) : ctx.layerTextures.find(handle);
}

std::shared_ptr<const Picture> loadPicture(const std::filesystem::path& path, std::string& error) {
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    if (code) {
        error = "the file couldn't be found";
        return nullptr;
    }
    if (size > MAX_PICTURE_FILE_SIZE) {
        error = "the file is larger than 256 MB";
        return nullptr;
    }

    auto picture = std::make_shared<Picture>();
    picture->fileName = utf8(path.filename());
    picture->png.resize(static_cast<std::size_t>(size));

    std::ifstream file(path, std::ios::binary);
    if (!file || !file.read(reinterpret_cast<char*>(picture->png.data()), static_cast<std::streamsize>(size))) {
        error = "the file couldn't be read";
        return nullptr;
    }

    if (!image::isPng(picture->png.data(), picture->png.size())) {
        error = "it isn't a PNG (only PNG images can be used for now)";
        return nullptr;
    }

    // Decoded once here to check it; the texture decodes it again when it's first drawn
    image::Image decoded;
    if (!image::decodePng(picture->png.data(), picture->png.size(), decoded, error)) return nullptr;

    picture->width = decoded.width;
    picture->height = decoded.height;
    return picture;
}
