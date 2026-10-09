#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "scene/pictures/picture.hpp"

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

// Pictures bigger than this aren't read (a PNG this large would also be far too big to decode)
inline constexpr std::uintmax_t MAX_PICTURE_FILE_SIZE = 256ull * 1024 * 1024;

// Reads and checks a PNG; on failure error says why
std::shared_ptr<const Picture> loadPicture(const std::filesystem::path& path, std::string& error);

// A path as UTF-8 text
std::string utf8(const std::filesystem::path& path);
