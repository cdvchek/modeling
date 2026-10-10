#include "application/paint/paint_workspace.hpp"
#include "application/ui/modal_windows.hpp"
#include "application/ui/ui_undo.hpp"
#include "application/commands/layer_commands.hpp"
#include "application/uv/uv_editor.hpp"
#include "application/viewport/material_view.hpp"
#include "scene/picking/ray.hpp"
#include "scene/picking/scene_queries.hpp"
#include "scene/textures/paint_targets.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
    constexpr u32 SIZES[] = { 256, 512, 1024, 2048 };
    constexpr i32 DEFAULT_SIZE = 2;
    constexpr f32 WINDOW_WIDTH = 380.0f;
    constexpr f32 BUTTON_WIDTH = 96.0f;
    constexpr f32 BUTTON_GAP = 8.0f;
    constexpr f32 WIRE_WIDTH = 1.5f;
    // [ and ] change the brush by this much, and by a pixel at least
    constexpr f32 BRUSH_STEP = 1.15f;
    constexpr int CURSOR_SEGMENTS = 48;

    // On the model a stroke is walked across the screen: a ray this many pixels apart, and no more than this many a frame
    constexpr f32 SAMPLE_PIXELS = 3.0f;
    constexpr int MAX_SAMPLES = 64;
    // Two samples further apart in the world than this many times what their screen distance covers there aren't joined
    constexpr f32 MAX_SURFACE_GAP = 6.0f;
    // Masks kept for the islands a stroke has crossed; the oldest goes first
    constexpr std::size_t MAX_STROKE_ISLANDS = 8;

    // The brush's outline at the mouse: light over dark, so it shows on any picture
    const Color CURSOR_LIGHT { 1.0f, 1.0f, 1.0f, 0.9f };
    const Color CURSOR_DARK { 0.0f, 0.0f, 0.0f, 0.6f };

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

namespace {
    // Nothing else owns the mouse: no open list from the header, console, or modal window
    bool canvasFree(const AppContext& ctx) {
        const ContextManager& contexts = ctx.systems.input_ctx;
        return !ctx.ui.popupOpen() && !contexts.isActive(InputContext_Console) && !contexts.isActive(InputContext_Modal);
    }

    Vec2 mousePosition(const AppContext& ctx) {
        return Vec2(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    }

    // The mouse as a point on the texture, in its pixels
    Vec2 mouseOnTexture(const AppContext& ctx, const Rect& area, const Texture& texture) {
        const Vec2 uv = screenToUV(ctx.workspace.paintView, area, mousePosition(ctx));
        return Vec2(uv.x * static_cast<f32>(texture.width()), uv.y * static_cast<f32>(texture.height()));
    }
}

namespace {
    // One point of a stroke on the model: what's under that spot of the 3D view gets the brush, on its place in the texture
    void sampleModel(AppContext& ctx, TextureHandle handle, Vec2 screen, f32 stepPixels) {
        Stroke& stroke = ctx.paintStroke;
        ModelStroke& model = ctx.modelStroke;
        const Rect view = sceneView(ctx);
        const ObjectHandle objectHandle = uvObject(ctx);
        const Object* object = ctx.scene.objects.tryGet(objectHandle);
        Texture& texture = ctx.scene.textures.get(handle);

        // Off the model, or on a face that draws with another texture: the brush is up until it's back
        FaceHit hit;
        if (object && view.contains(screen)) {
            const Ray ray = makeRayFromScreenPosition(static_cast<i32>(screen.x - view.x), static_cast<i32>(screen.y - view.y),
                                                      static_cast<u32>(view.width), static_cast<u32>(view.height), ctx.scene.camera);
            hit = pickFace(ctx.scene, ray, objectHandle, INVALID_OBJECT, [&ctx](ObjectHandle culled) { return backFacesCulled(ctx, culled); });
        }
        if (!hit.hit || !facePaints(ctx.scene, *object, hit.face, handle)) {
            stroke.lift();
            model.touching = false;
            return;
        }

        // The island under the brush, with its mask: made the first time the stroke reaches it
        std::size_t island = 0;
        const auto holds = [&](const ModelStroke::Island& known) {
            return std::binary_search(known.faces.begin(), known.faces.end(), hit.face, [](FaceHandle a, FaceHandle b) { return a.index < b.index; });
        };
        while (island < model.islands.size() && !holds(model.islands[island])) ++island;
        if (island == model.islands.size()) {
            if (model.islands.size() >= MAX_STROKE_ISLANDS) {
                model.islands.erase(model.islands.begin());
                model.touching = false;
                --island;
            }
            ModelStroke::Island made;
            made.faces = paintIsland(ctx.scene, *object, hit.face, handle);
            made.mask = std::make_shared<const PaintMask>(maskFromFaces(object->meshData, made.faces, texture.width(), texture.height()));
            model.islands.push_back(std::move(made));
        }

        // A line in the texture joins two samples only on one island and close together on the surface: never across a seam or a jump in depth
        const f32 pixelSize = hit.distance * 2.0f * std::tan(ctx.scene.camera.fovRadians * 0.5f) / std::max(view.height, 1.0f);
        const bool joined = model.touching && model.island == island
            && (hit.point - model.lastPoint).length() <= MAX_SURFACE_GAP * pixelSize * std::max(stepPixels, 1.0f);
        if (!joined) stroke.lift();

        stroke.setMask(model.islands[island].mask);
        stroke.moveTo(texture.layers, texturePoint(hit.uv, texture.width(), texture.height()));
        model.touching = true;
        model.island = island;
        model.lastPoint = hit.point;
    }

    // Carries a stroke on the model to the mouse, sampling the way there from where it was last frame
    void strokeOnModel(AppContext& ctx, TextureHandle handle) {
        ModelStroke& model = ctx.modelStroke;
        const Vec2 mouse = mousePosition(ctx);
        const Vec2 from = model.moved ? model.lastMouse : mouse;
        const f32 length = (mouse - from).length();
        const int steps = std::clamp(static_cast<int>(std::ceil(length / SAMPLE_PIXELS)), 1, MAX_SAMPLES);
        for (int i = 1; i <= steps; ++i) sampleModel(ctx, handle, from + (mouse - from) * (static_cast<f32>(i) / static_cast<f32>(steps)), length / static_cast<f32>(steps));
        model.lastMouse = mouse;
        model.moved = true;
    }
}

bool updatePaintStroke(AppContext& ctx) {
    Stroke& stroke = ctx.paintStroke;
    const InputState& input = ctx.systems.input;
    const u16 left = static_cast<u16>(MouseButton::Left);
    const bool flat = ctx.workspace.paint2D;
    const Rect area = flat ? screenLayout(ctx).paintCanvas : sceneView(ctx);
    const TextureHandle handle = activePaintTexture(ctx);

    if (stroke.active()) {
        // The texture can't change under a stroke, since nothing else gets input, but an ended one must never write
        Texture* texture = ctx.scene.textures.tryGet(handle);
        if (texture && texture->layered() && input.isMouseDown(left)) {
            if (ctx.modelStroke.onModel) strokeOnModel(ctx, handle);
            else stroke.moveTo(texture->layers, mouseOnTexture(ctx, area, *texture));
            return true;
        }

        // Released: the stroke is one undo step, or nothing if it never touched a pixel
        if (stroke.painted()) ctx.history.commit();
        else ctx.history.cancel(ctx.scene);
        stroke.end();
        ctx.modelStroke = {};
        return false;
    }

    const bool pressed = input.wasMousePressedThisFrame(left) && !input.isKeyDown(static_cast<u16>(Key::LeftAlt));
    if (!pressed || (flat && ctx.workspace.paintView.zoom <= 0.0f) || !canvasFree(ctx) || !area.contains(mousePosition(ctx))) return false;

    Texture* texture = ctx.scene.textures.tryGet(handle);
    if (!texture) {
        ctx.systems.console.printError("There's no texture to paint: make one with Texture in the header");
        return false;
    }
    if (texture->layered() && !texture->layers.layers[texture->layers.active].visible) {
        ctx.systems.console.printError(texture->layers.layers[texture->layers.active].name + " is hidden: show it to paint on it");
        return false;
    }

    // Giving a texture its layers is part of the stroke's undo step
    ctx.history.begin(ctx.scene);
    LayerStack* layers = paintLayers(ctx);
    if (!layers) {
        ctx.history.cancel(ctx.scene);
        return false;
    }
    stroke.begin(*layers, layers->active, ctx.workspace.brush);
    ctx.modelStroke = {};
    ctx.modelStroke.onModel = !flat;
    if (flat) stroke.moveTo(*layers, mouseOnTexture(ctx, area, ctx.scene.textures.get(handle)));
    else strokeOnModel(ctx, handle);
    return true;
}

void resizeBrush(AppContext& ctx, bool larger) {
    Brush& brush = ctx.workspace.brush;
    const f32 scaled = larger ? std::max(brush.size * BRUSH_STEP, brush.size + 1.0f) : std::min(brush.size / BRUSH_STEP, brush.size - 1.0f);
    brush.size = std::clamp(std::round(scaled), MIN_BRUSH_SIZE, MAX_BRUSH_SIZE);
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
    UIDrawList& list = ctx.ui.drawList();
    drawTextureSquare(list, view, area, textureImage(ctx, active), false);

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

    // The brush's size at the mouse; a texture that isn't square squashes it the way the picture is squashed
    const Texture* texture = ctx.scene.textures.tryGet(active);
    const Vec2 mouse = mousePosition(ctx);
    if (!texture || texture->width() == 0 || !area.contains(mouse) || (!canvasFree(ctx) && !ctx.paintStroke.active())) return;
    const f32 radius = ctx.workspace.brush.size * 0.5f * view.zoom;
    const Vec2 radii(radius / static_cast<f32>(texture->width()), radius / static_cast<f32>(texture->height()));
    list.pushClip(area);
    for (const auto& [width, color] : { std::pair { 3.0f, CURSOR_DARK }, std::pair { 1.0f, CURSOR_LIGHT } }) {
        for (int i = 0; i < CURSOR_SEGMENTS; ++i) {
            const f32 from = 6.2831853f * static_cast<f32>(i) / CURSOR_SEGMENTS, to = 6.2831853f * static_cast<f32>(i + 1) / CURSOR_SEGMENTS;
            list.line(mouse + Vec2(std::cos(from) * radii.x, std::sin(from) * radii.y), mouse + Vec2(std::cos(to) * radii.x, std::sin(to) * radii.y), width, color);
        }
    }
    list.popClip();
}

void drawPaintModelCursor(AppContext& ctx, const Rect& view) {
    const Vec2 mouse = mousePosition(ctx);
    const TextureHandle handle = activePaintTexture(ctx);
    const Texture* texture = ctx.scene.textures.tryGet(handle);
    const ObjectHandle objectHandle = uvObject(ctx);
    const Object* object = ctx.scene.objects.tryGet(objectHandle);
    if (!texture || texture->width() == 0 || !object || !view.contains(mouse) || (!canvasFree(ctx) && !ctx.paintStroke.active())) return;

    // Only over a face that would take the paint
    const Ray ray = makeRayFromScreenPosition(static_cast<i32>(mouse.x - view.x), static_cast<i32>(mouse.y - view.y),
                                              static_cast<u32>(view.width), static_cast<u32>(view.height), ctx.scene.camera);
    const FaceHit hit = pickFace(ctx.scene, ray, objectHandle, INVALID_OBJECT, [&ctx](ObjectHandle culled) { return backFacesCulled(ctx, culled); });
    Vec3 perX, perY;
    if (!hit.hit || !facePaints(ctx.scene, *object, hit.face, handle)) return;
    if (!textureAxes(hit.corners, hit.cornerUVs, texture->width(), texture->height(), perX, perY)) return;

    // The dab's circle on the texture, carried onto the face's plane and then to the screen
    const f32 radius = ctx.workspace.brush.size * 0.5f;
    const Mat4 viewProjection = sceneViewProjection(ctx);
    Vec2 points[CURSOR_SEGMENTS];
    for (int i = 0; i < CURSOR_SEGMENTS; ++i) {
        const f32 angle = 6.2831853f * static_cast<f32>(i) / CURSOR_SEGMENTS;
        if (!projectToView(viewProjection, hit.point + (perX * std::cos(angle) + perY * std::sin(angle)) * radius, view, points[i])) return;
    }

    UIDrawList& list = ctx.ui.drawList();
    list.pushClip(view);
    for (const auto& [width, color] : { std::pair { 3.0f, CURSOR_DARK }, std::pair { 1.0f, CURSOR_LIGHT } }) {
        for (int i = 0; i < CURSOR_SEGMENTS; ++i) list.line(points[i], points[(i + 1) % CURSOR_SEGMENTS], width, color);
    }
    list.popClip();
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

namespace {
    // The layer list shows this many rows before it scrolls
    constexpr u32 LAYER_ROWS = 6;

    const std::vector<std::string_view> TOOLS = { "Brush", "Eraser" };

    // Brush or eraser, and its color, size, softness, opacity, and spacing; none of it is part of undo
    void brushSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        Brush& brush = ctx.workspace.brush;
        ui.heading("Brush");
        ui.pushId("brush");

        i32 tool = brush.erase ? 1 : 0;
        if (ui.segmented("Tool", tool, TOOLS)) brush.erase = tool == 1;
        ui.colorEdit("Color", brush.color);
        ui.sliderFloat("Size", brush.size, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE, "%.0f px");
        ui.sliderFloat("Softness", brush.softness, 0.0f, 1.0f);
        ui.sliderFloat("Opacity", brush.opacity, 0.0f, 1.0f);

        // Shown in percent of the brush's size
        f32 spacing = brush.spacing * 100.0f;
        if (ui.sliderFloat("Spacing", spacing, MIN_BRUSH_SPACING * 100.0f, MAX_BRUSH_SPACING * 100.0f, "%.0f %%")) brush.spacing = spacing / 100.0f;
        brush.clamp();
        ui.popId();
    }

    // The layers of the texture being painted, top first, with + and -, then the active layer's name, opacity, visibility, and place
    void layersSection(AppContext& ctx, TextureHandle handle) {
        UIContext& ui = ctx.ui;
        // Looked up again after every change: an undo restore in the middle of a frame replaces the textures
        const auto layers = [&]() -> LayerStack* {
            Texture* texture = ctx.scene.textures.tryGet(handle);
            return texture ? &texture->layers : nullptr;
        };
        const auto layerAt = [&](u32 index) -> Layer* {
            LayerStack* stack = layers();
            return stack && index < stack->layers.size() ? &stack->layers[index] : nullptr;
        };
        // What this frame draws; the copy shares its tiles
        const LayerStack shown = *layers();
        const u32 count = static_cast<u32>(shown.layers.size());
        const u32 active = shown.active;

        const Rect header = ui.row();
        const Rect minus { header.right() - header.height, header.y, header.height, header.height };
        const Rect plus { minus.x - UIStyle::COMPONENT_GAP - header.height, header.y, header.height, header.height };
        ui.text(header, "Layers", UIStyle::ACCENT_GREEN);

        // + turns a texture that's still one picture into layers first, in the same undo step
        if (ui.button("+", plus)) {
            ctx.history.begin(ctx.scene);
            if (LayerStack* stack = paintLayers(ctx)) {
                addLayer(*stack);
                ctx.history.commit();
            } else {
                ctx.history.cancel(ctx.scene);
            }
        }
        if (ui.button("-", minus, count > 1)) {
            ctx.history.begin(ctx.scene);
            removeLayer(*layers(), active);
            ctx.history.commit();
        }

        ui.beginChild("layers", LAYER_ROWS * UIStyle::ROW_HEIGHT + (LAYER_ROWS - 1) * UIStyle::ITEM_SPACING + UIStyle::CHILD_PADDING * 2.0f);
        if (count == 0) ui.selectable("Base", true, "picture");
        for (u32 i = count; i-- > 0;) {
            const Layer& layer = shown.layers[i];
            const std::string detail = !layer.visible ? "hidden" : layer.opacity < 1.0f ? std::to_string(std::lround(layer.opacity * 100.0f)) + "%" : "";
            ui.pushId(i);
            if (ui.selectable(layer.name, i == active, detail)) {
                if (LayerStack* stack = layers()) stack->active = i;
            }
            ui.popId();
        }
        ui.endChild();

        if (count == 0) {
            ui.label("+ adds a layer over it", true);
            return;
        }

        // The active layer; each edit is one undo step
        ui.pushId("layer");
        std::string name = shown.layers[active].name;
        if (ui.textField("Name", name)) {
            if (Layer* layer = layerAt(active)) layer->name = name;
        }
        trackUndo(ctx);

        f32 opacity = shown.layers[active].opacity;
        if (ui.sliderFloat("Opacity", opacity, 0.0f, 1.0f)) {
            if (Layer* layer = layerAt(active)) layer->opacity = opacity;
        }
        trackUndo(ctx);

        bool visible = shown.layers[active].visible;
        if (ui.checkbox("Visible", visible)) {
            if (Layer* layer = layerAt(active)) layer->visible = visible;
        }
        trackUndo(ctx);

        // Up is toward the top of the stack
        const Rect row = ui.row();
        const f32 half = std::floor((row.width - UIStyle::COMPONENT_GAP) * 0.5f);
        const auto move = [&](u32 to) {
            ctx.history.begin(ctx.scene);
            moveLayer(*layers(), active, to);
            ctx.history.commit();
        };
        if (ui.button("Up", { row.x, row.y, half, row.height }, active + 1 < count)) move(active + 1);
        if (ui.button("Down", { row.x + half + UIStyle::COMPONENT_GAP, row.y, row.width - half - UIStyle::COMPONENT_GAP, row.height }, active > 0)) move(active - 1);
        ui.popId();
    }
}

std::string textureSizeText(const Texture& texture) {
    if (texture.width() == 0) return "";
    return std::to_string(texture.width()) + " x " + std::to_string(texture.height());
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
    brushSection(ctx);
    ui.spacing();
    layersSection(ctx, active);
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
    // Made to be painted, so it has its Base layer from the start
    texture.layers = solidLayers(width, height, target->baseColor);

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
