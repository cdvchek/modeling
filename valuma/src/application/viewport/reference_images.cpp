#include "application/viewport/reference_images.hpp"
#include "application/workspace.hpp"
#include "application/app_context.hpp"
#include "application/actions/project_actions.hpp"
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

Vec3 rotationFacingView(const AppContext& ctx) {
    const Camera& camera = ctx.scene.camera;
    const Vec3 forward = camera.getForward().normalized();
    const Vec3 right = camera.getRight().normalized();
    const Vec3 up = Vec3::cross(right, forward).normalized();
    return eulerFromAxes(right, up, -forward);
}

ReferenceHandle addReferenceImage(AppContext& ctx, std::shared_ptr<const Picture> picture, const std::string& name) {
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
    std::shared_ptr<const Picture> picture = loadPicture(path, error);
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

    for (const auto& [distance, handle] : order) drawReferenceImage(ctx, viewProjection, handle);
}

void drawReferenceImage(AppContext& ctx, const Mat4& viewProjection, ReferenceHandle handle) {
    const ReferenceImage& image = ctx.scene.references.get(handle);

    DrawImageCommand command;
    command.texture = ctx.pictureTextures.sync(ctx, image.picture);
    command.mvp = viewProjection * image.matrix();
    command.opacity = std::clamp(image.opacity, 0.0f, 1.0f);
    command.depthTest = image.depth == ReferenceDepth::InScene;
    ctx.renderer->drawImage(command);
}

void drawReferenceOutlines(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, const Rect& view) {
    for (ReferenceHandle handle : ctx.scene.selection.getReferences()) {
        const ReferenceImage* image = ctx.scene.references.tryGet(handle);
        if (!image || !image->visible) continue;

        // Skipped when a corner is behind the camera
        Vec2 screen[4];
        bool onScreen = true;
        const std::vector<Vec3> world = corners(*image);
        for (u32 i = 0; i < 4 && onScreen; ++i) onScreen = projectToView(viewProjection, world[i], view, screen[i]);
        if (!onScreen) continue;

        for (u32 i = 0; i < 4; ++i) ui.line(screen[i], screen[(i + 1) % 4], OUTLINE_HALO_WIDTH, OUTLINE_HALO);
        for (u32 i = 0; i < 4; ++i) ui.line(screen[i], screen[(i + 1) % 4], OUTLINE_WIDTH, UIStyle::ACCENT);
    }
}
