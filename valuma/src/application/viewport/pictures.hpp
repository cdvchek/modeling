#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "scene/pictures/picture.hpp"
#include "scene/textures/texture.hpp"

struct AppContext;
class IRenderer;

// GPU textures for pictures (reference images and textures): a picture is decoded and uploaded the first time it's
// needed, and its texture is freed once nothing holds the picture any more (not the scene, and not an undo step)
class PictureTextureCache {
public:
    // 0 when the picture can't be shown (damaged, or too large for the GPU); that's reported once
    u32 sync(AppContext& ctx, const std::shared_ptr<const Picture>& picture);
    // The texture if it's been uploaded, otherwise 0; for code that can't upload (drawing from a const context)
    u32 find(const std::shared_ptr<const Picture>& picture) const;
    void prune(IRenderer& renderer);

private:
    struct Entry {
        std::weak_ptr<const Picture> picture;
        u32 texture = 0;
    };

    std::vector<Entry> m_entries;
};

// GPU textures for layered textures: each shows its layers' combined picture, and only the tiles that changed are sent again
class LayerTextureCache {
public:
    // Uploads what changed since the last call (paint, layer edits, undo) and frees what's gone; call before anything draws
    void sync(AppContext& ctx);
    // 0 when the texture has no layers or couldn't be uploaded
    u32 find(TextureHandle handle) const;
    // The last sync sent this texture new pixels, so what's rendered from it (swatches) is out of date
    bool changed(TextureHandle handle) const;

private:
    struct Entry {
        TextureHandle handle;
        // What the GPU has; holding it also makes the next change to a tile go to a copy, which is how changes are found
        LayerStack shown;
        u32 texture = 0;
        bool changed = false;
    };

    std::vector<Entry> m_entries;
    std::vector<u8> m_pixels;
};

// The GPU texture that shows a texture: its layers' combined picture when it has layers, otherwise its picture; 0 when there's none yet
u32 textureImage(const AppContext& ctx, TextureHandle handle);

// Pictures bigger than this aren't read (a PNG this large would also be far too big to decode)
inline constexpr std::uintmax_t MAX_PICTURE_FILE_SIZE = 256ull * 1024 * 1024;

// Reads and checks a PNG; on failure error says why
std::shared_ptr<const Picture> loadPicture(const std::filesystem::path& path, std::string& error);

// A path as UTF-8 text
std::string utf8(const std::filesystem::path& path);
