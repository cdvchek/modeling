#include "application/paint/paint_workspace.hpp"
#include "application/ui/modal_windows.hpp"
#include "application/uv/uv_editor.hpp"
#include "application/viewport/material_view.hpp"
#include "scene/picking/ray.hpp"
#include "scene/picking/scene_queries.hpp"
#include "scene/textures/paint_targets.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cfloat>

namespace {
    constexpr u32 SIZES[] = { 256, 512, 1024, 2048 };
    constexpr i32 DEFAULT_SIZE = 2;
    constexpr f32 WINDOW_WIDTH = 380.0f;
    constexpr f32 BUTTON_WIDTH = 96.0f;
    constexpr f32 BUTTON_GAP = 8.0f;
    constexpr f32 WIRE_WIDTH = 1.5f;

    const Color WIRE { 0.90f, 0.91f, 0.95f, 0.75f };
    // Faces of other textures: shaded over, with a faint outline
    const Color OTHER_FILL { 0.06f, 0.06f, 0.08f, 0.55f };
    const Color OTHER_WIRE { 0.60f, 0.61f, 0.68f, 0.22f };

    const std::vector<std::string_view> SIZE_NAMES = { "256", "512", "1024", "2048" };

    // Each face's UV at every corner of its triangles
    template <typename Visit>
    void forEachTriangle(const MeshData& mesh, FaceHandle face, Visit visit) {
        const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        const auto uvOf = [&](VertexHandle vertex) {
            for (std::size_t i = 0; i < vertices.size() && i < uvs.size(); ++i) if (vertices[i] == vertex) return uvs[i];
            return Vec2();
        };
        for (const Triangle& triangle : mesh.getFaceTriangles(face)) visit(uvOf(triangle.v0), uvOf(triangle.v1), uvOf(triangle.v2));
    }

    void outline(UIDrawList& list, const UVView& view, const Rect& area, const std::vector<Vec2>& uvs, f32 width, const Color& color) {
        for (std::size_t i = 0; i < uvs.size(); ++i) {
            const Vec2 from = uvToScreen(view, area, uvs[(i + uvs.size() - 1) % uvs.size()]);
            list.line(from, uvToScreen(view, area, uvs[i]), width, color);
        }
    }

    std::string materialNames(const AppContext& ctx, const std::vector<MaterialHandle>& materials) {
        std::string names;
        for (MaterialHandle material : materials) {
            if (!names.empty()) names += ", ";
            names += ctx.scene.materials.get(material).name;
        }
        return names;
    }
}

TextureHandle activePaintTexture(const AppContext& ctx) {
    const std::vector<PaintTarget> targets = paintTargets(ctx.scene, uvObject(ctx));
    for (const PaintTarget& target : targets) {
        if (target.texture == ctx.workspace.paintTexture && ctx.scene.textures.isValid(target.texture)) return target.texture;
    }
    // Mapped targets come first
    return targets.empty() ? INVALID_TEXTURE : targets.front().texture;
}

void togglePaintView(AppContext& ctx) {
    ctx.workspace.paint2D = !ctx.workspace.paint2D;
    ctx.workspace.paintView.panning = false;
}

bool canPickPaintTexture(const AppContext& ctx) {
    return !ctx.workspace.paint2D && ctx.scene.objects.isValid(uvObject(ctx));
}

void pickPaintTexture(AppContext& ctx) {
    const Rect view = sceneView(ctx);
    const InputState& input = ctx.systems.input;
    const Vec2 mouse(static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()));
    if (!view.contains(mouse)) return;

    const Ray ray = makeRayFromScreenPosition(static_cast<i32>(mouse.x - view.x), static_cast<i32>(mouse.y - view.y),
                                              static_cast<u32>(view.width), static_cast<u32>(view.height), ctx.scene.camera);
    const ObjectHandle handle = uvObject(ctx);
    const FaceHit hit = pickFace(ctx.scene, ray, handle, INVALID_OBJECT, [&ctx](ObjectHandle object) { return backFacesCulled(ctx, object); });
    if (!hit.hit) return;

    const Material& material = ctx.scene.materials.get(faceDrawMaterial(ctx.scene, ctx.scene.objects.get(handle), hit.face));
    if (!ctx.scene.textures.isValid(material.baseColorMap)) {
        ctx.systems.console.printError(material.name + " has no texture: pick New texture for " + material.name + " in the header");
        return;
    }
    ctx.workspace.paintTexture = material.baseColorMap;
}

void updatePaintCanvas(AppContext& ctx) {
    const Rect area = screenLayout(ctx).paintCanvas;
    if (area.width > 0.0f && area.height > 0.0f) navigateUVView(ctx, ctx.workspace.paintView, area);
}

void drawPaintCanvas(AppContext& ctx, const Rect& area) {
    UVView& view = ctx.workspace.paintView;
    if (view.zoom <= 0.0f) fitUVView(view, area, Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f));

    const TextureHandle active = activePaintTexture(ctx);
    const Texture* texture = ctx.scene.textures.tryGet(active);
    UIDrawList& list = ctx.ui.drawList();
    drawTextureSquare(list, view, area, texture ? ctx.pictureTextures.find(texture->picture) : 0, false);

    const ObjectHandle handle = uvObject(ctx);
    const Object* object = ctx.scene.objects.tryGet(handle);
    if (!object) return;
    const MeshData& mesh = object->meshData;

    // Other textures' faces first, shaded over so they read as out of reach; the paintable ones' outlines on top
    std::vector<FaceHandle> paintable;
    for (FaceHandle face : mesh.getFaceHandles()) {
        if (facePaints(ctx.scene, *object, face, active)) {
            paintable.push_back(face);
            continue;
        }
        forEachTriangle(mesh, face, [&](Vec2 a, Vec2 b, Vec2 c) {
            list.triangle(uvToScreen(view, area, a), uvToScreen(view, area, b), uvToScreen(view, area, c), OTHER_FILL);
        });
        outline(list, view, area, mesh.getFaceUVs(face), 1.0f, OTHER_WIRE);
    }
    for (FaceHandle face : paintable) outline(list, view, area, mesh.getFaceUVs(face), WIRE_WIDTH, WIRE);
}

void framePaintCanvas(AppContext& ctx, bool all) {
    const Rect area = screenLayout(ctx).paintCanvas;
    if (area.width <= 0.0f || area.height <= 0.0f) return;

    // The paintable faces' UVs
    Vec2 low(FLT_MAX, FLT_MAX), high(-FLT_MAX, -FLT_MAX);
    const Object* object = ctx.scene.objects.tryGet(uvObject(ctx));
    const TextureHandle active = activePaintTexture(ctx);
    if (object && !all) {
        for (FaceHandle face : object->meshData.getFaceHandles()) {
            if (!facePaints(ctx.scene, *object, face, active)) continue;
            for (const Vec2& uv : object->meshData.getFaceUVs(face)) {
                low = Vec2(std::min(low.x, uv.x), std::min(low.y, uv.y));
                high = Vec2(std::max(high.x, uv.x), std::max(high.y, uv.y));
            }
        }
    }
    if (low.x > high.x) {
        low = Vec2(0.0f, 0.0f);
        high = Vec2(1.0f, 1.0f);
    }
    fitUVView(ctx.workspace.paintView, area, low, high);
}

std::string textureSizeText(const Texture& texture) {
    if (!texture.picture) return "";
    return std::to_string(texture.picture->width) + " x " + std::to_string(texture.picture->height);
}

void drawPaintToolsPanel(AppContext& ctx, const Rect& area) {
    UIContext& ui = ctx.ui;
    ui.drawList().rect(area, UIStyle::PANEL_BACKGROUND);
    ui.drawList().rect({ area.x, area.y, 1.0f, area.height }, UIStyle::PANEL_BORDER);

    // What's being painted, and where it shows
    ui.heading("Texture");
    const TextureHandle active = activePaintTexture(ctx);
    const Texture* texture = ctx.scene.textures.tryGet(active);
    if (!texture) {
        ui.label("No texture yet", true);
        ui.label("Make one: Texture", true);
        ui.label("in the header", true);
        return;
    }
    ui.label(texture->name);
    ui.label(textureSizeText(*texture), true);
    for (const PaintTarget& target : paintTargets(ctx.scene, uvObject(ctx))) {
        if (target.texture == active) ui.label("On " + materialNames(ctx, target.materials), true);
    }
    ui.spacing();
    ui.label("Alt+click a face", true);
    ui.label("to paint its", true);
    ui.label("texture", true);
}

void openNewTextureWindow(AppContext& ctx, MaterialHandle material) {
    if (!ctx.scene.materials.isValid(material)) return;
    NewTextureState& state = ctx.modal.newTexture;
    state = {};
    state.material = material;
    state.width = DEFAULT_SIZE;
    state.height = DEFAULT_SIZE;
    openModal(ctx, ModalKind::NewTexture);
}

TextureHandle createSolidTexture(AppContext& ctx, MaterialHandle material, u32 width, u32 height) {
    Material* target = ctx.scene.materials.tryGet(material);
    if (!target) return INVALID_TEXTURE;

    Texture texture;
    texture.name = ctx.scene.textures.uniqueName(target->name);
    texture.picture = solidPicture(texture.name + ".png", width, height, target->baseColor);

    // The texture holds the color now; white keeps the look the same, since the base color multiplies the map
    ctx.history.begin(ctx.scene);
    const TextureHandle handle = ctx.scene.textures.add(std::move(texture));
    target = ctx.scene.materials.tryGet(material);
    target->baseColorMap = handle;
    target->baseColor = Vec3(1.0f, 1.0f, 1.0f);
    ctx.history.commit();

    ctx.workspace.paintTexture = handle;
    ctx.viewport.selectedTexture = handle;
    const Texture& made = ctx.scene.textures.get(handle);
    ctx.systems.console.print("Made " + made.name + " (" + textureSizeText(made) + ") for " + target->name);
    return handle;
}

void drawNewTextureWindow(AppContext& ctx, const Rect& viewport) {
    UIContext& ui = ctx.ui;
    NewTextureState& state = ctx.modal.newTexture;
    const Material* material = ctx.scene.materials.tryGet(state.material);
    if (!material) {
        state.closeRequested = true;
        return;
    }

    const f32 height = UIStyle::PANEL_HEADER_HEIGHT + UIStyle::PADDING * 2.0f + UIStyle::ROW_HEIGHT * 5.0f + UIStyle::ITEM_SPACING * 4.0f + UIStyle::SECTION_SPACING;
    ui.beginModal("new texture", viewport, WINDOW_WIDTH, height, "New texture");
    ui.label("For " + material->name);
    ui.label("Filled with its base color", true);
    ui.segmented("Width", state.width, SIZE_NAMES);
    ui.segmented("Height", state.height, SIZE_NAMES);
    ui.spacing();

    // Create (the default, Enter) and Cancel (Escape) on the right
    const Rect row = ui.row();
    const Rect cancel { row.right() - BUTTON_WIDTH, row.y, BUTTON_WIDTH, row.height };
    const Rect create { cancel.x - BUTTON_GAP - BUTTON_WIDTH, row.y, BUTTON_WIDTH, row.height };
    if (ui.button("Create", create)) state.createRequested = true;
    ui.drawList().roundedRect(create, UIStyle::CORNER_RADIUS, { 0.0f, 0.0f, 0.0f, 0.0f }, UIStyle::ACCENT, 1.0f);
    if (ui.button("Cancel", cancel)) state.closeRequested = true;
    ui.endModal();
}

void updateNewTextureWindow(AppContext& ctx) {
    NewTextureState state = ctx.modal.newTexture;
    if (!state.createRequested && !state.closeRequested) return;

    closeModal(ctx);
    ctx.modal.newTexture = {};
    if (state.createRequested) {
        const auto size = [](i32 index) { return SIZES[std::clamp(index, 0, static_cast<i32>(std::size(SIZES)) - 1)]; };
        createSolidTexture(ctx, state.material, size(state.width), size(state.height));
    }
}

void confirmNewTextureWindow(AppContext& ctx) {
    ctx.modal.newTexture.createRequested = true;
}
