#include "application/reference_images.hpp"
#include "application/app_context.hpp"
#include "application/project_actions.hpp"
#include "core/math/projection.hpp"
#include "core/math/vec4.hpp"
#include "image/image.hpp"
#include "platform/platform.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <fstream>

namespace {
    constexpr const char* IMAGE_TYPE = "PNG image";
    constexpr f32 OUTLINE_WIDTH = 2.0f;
    constexpr f32 OUTLINE_HALO_WIDTH = 4.0f;
    const Color OUTLINE_HALO { 0.06f, 0.06f, 0.08f, 0.6f };

    std::string utf8(const std::filesystem::path& path) {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    bool samePicture(const std::weak_ptr<const ReferencePicture>& a, const std::shared_ptr<const ReferencePicture>& b) {
        return !a.owner_before(b) && !b.owner_before(a);
    }

    // The image's corners in the world: top left, top right, bottom right, bottom left
    std::vector<Vec3> corners(const ReferenceImage& image) {
        const Mat4 matrix = image.matrix();
        std::vector<Vec3> result;
        for (const Vec2 corner : { Vec2(-0.5f, 0.5f), Vec2(0.5f, 0.5f), Vec2(0.5f, -0.5f), Vec2(-0.5f, -0.5f) }) {
            const Vec4 world = matrix * Vec4(corner.x, corner.y, 0.0f, 1.0f);
            result.push_back(Vec3(world.x, world.y, world.z));
        }
        return result;
    }
}

u32 ReferenceTextureCache::sync(AppContext& ctx, const std::shared_ptr<const ReferencePicture>& picture) {
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

void ReferenceTextureCache::prune(IRenderer& renderer) {
    std::erase_if(m_entries, [&](const Entry& entry) {
        if (!entry.picture.expired()) return false;
        renderer.destroyTexture(entry.texture);
        return true;
    });
}

std::shared_ptr<const ReferencePicture> loadReferencePicture(const std::filesystem::path& path, std::string& error) {
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    if (code) {
        error = "the file couldn't be found";
        return nullptr;
    }
    if (size > MAX_REFERENCE_FILE_SIZE) {
        error = "the file is larger than 256 MB";
        return nullptr;
    }

    auto picture = std::make_shared<ReferencePicture>();
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

Vec3 rotationFacingView(const AppContext& ctx) {
    const Camera& camera = ctx.scene.camera;
    const Vec3 forward = camera.getForward().normalized();
    const Vec3 right = camera.getRight().normalized();
    const Vec3 up = Vec3::cross(right, forward).normalized();
    return eulerFromAxes(right, up, -forward);
}

ReferenceHandle addReferenceImage(AppContext& ctx, std::shared_ptr<const ReferencePicture> picture, const std::string& name) {
    ReferenceImage image;
    image.name = ctx.scene.references.uniqueName(name.empty() ? "Image" : name);
    image.picture = std::move(picture);
    image.position = ctx.scene.camera.target;
    image.rotation = rotationFacingView(ctx);

    ctx.history.begin(ctx.scene);
    const ReferenceHandle handle = ctx.scene.references.add(std::move(image));
    ctx.scene.selection.clear();
    ctx.scene.selection.addReference(handle);
    ctx.history.commit();
    return handle;
}

bool addReferenceFrom(AppContext& ctx, const std::filesystem::path& path) {
    const std::string fileName = utf8(path.filename());

    std::string error;
    std::shared_ptr<const ReferencePicture> picture = loadReferencePicture(path, error);
    if (!picture) {
        ctx.systems.console.printError("Couldn't add " + fileName + ": " + error);
        return false;
    }

    const std::string size = std::to_string(picture->width) + " x " + std::to_string(picture->height);
    const ReferenceHandle handle = addReferenceImage(ctx, std::move(picture), utf8(path.stem()));
    ctx.referenceFolder = path.parent_path();
    ctx.systems.console.print("Added " + ctx.scene.references.get(handle).name + " (" + fileName + ", " + size + ")");
    return true;
}

void chooseReferenceImages(AppContext& ctx) {
    std::error_code code;
    const std::filesystem::path folder = std::filesystem::is_directory(ctx.referenceFolder, code) ? ctx.referenceFolder : projectsFolder();

    const std::vector<std::filesystem::path> paths = Platform::chooseOpenFiles(nativeWindow(ctx), IMAGE_TYPE, "png", folder);
    afterDialog(ctx);

    for (const std::filesystem::path& path : paths) addReferenceFrom(ctx, path);
}

void deleteSelectedReferences(AppContext& ctx) {
    if (!ctx.scene.selection.hasReferences()) return;

    ctx.history.begin(ctx.scene);
    for (ReferenceHandle handle : ctx.scene.selection.getReferences()) {
        if (ctx.scene.references.isValid(handle)) ctx.scene.references.remove(handle);
    }
    ctx.scene.selection.clearReferences();
    ctx.history.commit();
}

void drawReferenceImages(AppContext& ctx, const Mat4& viewProjection, ReferenceDepth depth) {
    const Vec3 eye = ctx.scene.camera.position;

    std::vector<std::pair<f32, ReferenceHandle>> order;
    for (ReferenceHandle handle : ctx.scene.references.handles()) {
        const ReferenceImage& image = ctx.scene.references.get(handle);
        if (image.visible && image.depth == depth && image.opacity > 0.0f) order.push_back({ (image.position - eye).length(), handle });
    }
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    for (const auto& [distance, handle] : order) {
        const ReferenceImage& image = ctx.scene.references.get(handle);

        DrawImageCommand command;
        command.texture = ctx.referenceTextures.sync(ctx, image.picture);
        command.mvp = viewProjection * image.matrix();
        command.opacity = std::clamp(image.opacity, 0.0f, 1.0f);
        command.depthTest = depth == ReferenceDepth::InScene;
        ctx.renderer->drawImage(command);
    }
}

void drawReferenceOutlines(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, f32 width, f32 height) {
    for (ReferenceHandle handle : ctx.scene.selection.getReferences()) {
        const ReferenceImage* image = ctx.scene.references.tryGet(handle);
        if (!image || !image->visible) continue;

        // Skipped when a corner is behind the camera
        Vec2 screen[4];
        bool onScreen = true;
        const std::vector<Vec3> world = corners(*image);
        for (u32 i = 0; i < 4 && onScreen; ++i) onScreen = projectToScreen(viewProjection, world[i], width, height, screen[i]);
        if (!onScreen) continue;

        for (u32 i = 0; i < 4; ++i) ui.line(screen[i], screen[(i + 1) % 4], OUTLINE_HALO_WIDTH, OUTLINE_HALO);
        for (u32 i = 0; i < 4; ++i) ui.line(screen[i], screen[(i + 1) % 4], OUTLINE_WIDTH, UIStyle::ACCENT);
    }
}
