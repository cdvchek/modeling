#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "scene/references/reference_image.hpp"
#include "core/math/mat4.hpp"

struct AppContext;
class IRenderer;
class UIDrawList;

// GPU textures for reference pictures: a picture is decoded and uploaded the first time it's drawn, and its texture
// is freed once nothing holds the picture any more (not the scene, and not an undo step)
class ReferenceTextureCache {
public:
    // 0 when the picture can't be shown (damaged, or too large for the GPU); that's reported once
    u32 sync(AppContext& ctx, const std::shared_ptr<const ReferencePicture>& picture);
    void prune(IRenderer& renderer);

private:
    struct Entry {
        std::weak_ptr<const ReferencePicture> picture;
        u32 texture = 0;
    };

    std::vector<Entry> m_entries;
};

// Pictures bigger than this aren't read (a PNG this large would also be far too big to decode)
inline constexpr std::uintmax_t MAX_REFERENCE_FILE_SIZE = 256ull * 1024 * 1024;

// Reads and checks a PNG, ready to add; on failure error says why
std::shared_ptr<const ReferencePicture> loadReferencePicture(const std::filesystem::path& path, std::string& error);

// Adds the picture as a new image facing the view at the camera's target, selected; one undo step
ReferenceHandle addReferenceImage(AppContext& ctx, std::shared_ptr<const ReferencePicture> picture, const std::string& name);

// Adds a PNG file as a new reference image, reporting to the console
bool addReferenceFrom(AppContext& ctx, const std::filesystem::path& path);

// + in the Images tab: picks PNG files and adds each
void chooseReferenceImages(AppContext& ctx);

void deleteSelectedReferences(AppContext& ctx);

// Turns an image to face the view (its top toward the top of the screen), keeping where it is
Vec3 rotationFacingView(const AppContext& ctx);

// Draws the images in one depth group, farthest first so see-through ones blend over what's behind them.
// Images in the scene are sorted in with see-through objects instead, and drawn one at a time.
void drawReferenceImages(AppContext& ctx, const Mat4& viewProjection, ReferenceDepth depth);
void drawReferenceImage(AppContext& ctx, const Mat4& viewProjection, ReferenceHandle handle);

// Outlines selected images in the selection color
void drawReferenceOutlines(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, f32 width, f32 height);
