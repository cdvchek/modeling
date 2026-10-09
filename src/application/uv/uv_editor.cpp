#include "application/uv/uv_editor.hpp"
#include "application/viewport/material_view.hpp"
#include "core/math/vec4.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
    constexpr f32 MIN_ZOOM = 20.0f;         // pixels per UV unit
    constexpr f32 MAX_ZOOM = 200000.0f;
    constexpr f32 ZOOM_STEP = 1.15f;        // per wheel notch
    constexpr f32 WHEEL_NOTCH = 120.0f;     // the wheel reports this much per notch
    constexpr f32 FRAME_MARGIN = 0.9f;      // framed content fills this much of the view
    constexpr f32 MIN_FRAME_SIZE = 0.01f;   // in UV units, so a single point still frames sensibly
    constexpr u32 CHECKER_CELLS = 8;
    constexpr f32 GRID_STEP = 1.0f / 8.0f;  // matches the UV grid's checker cells
    constexpr f32 MIN_GRID_SPACING = 8.0f;  // pixels; closer grid lines aren't drawn
    constexpr f32 WIRE_WIDTH = 1.5f;

    const Color EDITOR_BACKGROUND { 0.10f, 0.10f, 0.13f, 1.0f };
    const Color CHECKER_LIGHT { 0.34f, 0.35f, 0.42f, 1.0f };
    const Color CHECKER_DARK { 0.25f, 0.26f, 0.32f, 1.0f };
    const Color GRID_LINE { 0.38f, 0.40f, 0.50f, 0.25f };
    const Color UNIT_LINE { 0.55f, 0.57f, 0.70f, 0.55f };
    const Color SQUARE_BORDER { 0.74f, 0.58f, 0.98f, 0.9f };
    const Color WIRE { 0.90f, 0.91f, 0.95f, 0.85f };

    // The selected corners' UVs, or every UV when nothing is selected or all is set
    bool uvBounds(const AppContext& ctx, bool all, Vec2& low, Vec2& high) {
        const ObjectHandle handle = uvObject(ctx);
        if (!ctx.scene.objects.isValid(handle)) return false;
        const MeshData& mesh = ctx.scene.objects.get(handle).meshData;
        const Selection& selection = ctx.scene.selection;

        low = Vec2(FLT_MAX, FLT_MAX);
        high = Vec2(-FLT_MAX, -FLT_MAX);
        const auto include = [&](const Vec2& uv) {
            low = Vec2(std::min(low.x, uv.x), std::min(low.y, uv.y));
            high = Vec2(std::max(high.x, uv.x), std::max(high.y, uv.y));
        };

        // Selected faces count with their own corners only; otherwise every corner at a selected vertex
        if (!all && selection.hasFaces()) {
            for (FaceHandle face : selection.getFaceHandles()) {
                for (const Vec2& uv : mesh.getFaceUVs(face)) include(uv);
            }
            if (low.x <= high.x) return true;
        }
        if (!all && selection.hasVertices()) {
            for (FaceHandle face : mesh.getFaceHandles()) {
                const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
                const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
                for (std::size_t i = 0; i < vertices.size() && i < uvs.size(); ++i) {
                    if (selection.hasVertex(handle, vertices[i])) include(uvs[i]);
                }
            }
            if (low.x <= high.x) return true;
        }

        // Everything: every UV and the texture itself
        include(Vec2(0.0f, 0.0f));
        include(Vec2(1.0f, 1.0f));
        for (const Vec2& uv : mesh.getCornerUVs()) include(uv);
        return true;
    }

    void fitUVs(WorkspaceState& workspace, const Rect& area, Vec2 low, Vec2 high) {
        const f32 width = std::max(high.x - low.x, MIN_FRAME_SIZE);
        const f32 height = std::max(high.y - low.y, MIN_FRAME_SIZE);
        workspace.uvCenter = (low + high) * 0.5f;
        workspace.uvZoom = std::clamp(std::min(area.width / width, area.height / height) * FRAME_MARGIN, MIN_ZOOM, MAX_ZOOM);
    }

    // Grid lines across the visible part of the editor, every step in UV units; whole units stronger
    void drawGrid(UIDrawList& list, const WorkspaceState& workspace, const Rect& area) {
        if (workspace.uvZoom * GRID_STEP < MIN_GRID_SPACING) return;
        const Vec2 topLeft = screenToUV(workspace, area, Vec2(area.x, area.y));
        const Vec2 bottomRight = screenToUV(workspace, area, Vec2(area.right(), area.bottom()));

        for (f32 u = std::floor(topLeft.x / GRID_STEP) * GRID_STEP; u <= bottomRight.x; u += GRID_STEP) {
            const f32 x = uvToScreen(workspace, area, Vec2(u, 0.0f)).x;
            const bool unit = std::fabs(u - std::round(u)) < 1e-4f;
            list.rect({ std::floor(x), area.y, 1.0f, area.height }, unit ? UNIT_LINE : GRID_LINE);
        }
        for (f32 v = std::floor(topLeft.y / GRID_STEP) * GRID_STEP; v <= bottomRight.y; v += GRID_STEP) {
            const f32 y = uvToScreen(workspace, area, Vec2(0.0f, v)).y;
            const bool unit = std::fabs(v - std::round(v)) < 1e-4f;
            list.rect({ area.x, std::floor(y), area.width, 1.0f }, unit ? UNIT_LINE : GRID_LINE);
        }
    }
}

Vec2 uvToScreen(const WorkspaceState& workspace, const Rect& area, Vec2 uv) {
    return area.center() + (uv - workspace.uvCenter) * workspace.uvZoom;
}

Vec2 screenToUV(const WorkspaceState& workspace, const Rect& area, Vec2 screen) {
    return workspace.uvCenter + (screen - area.center()) / std::max(workspace.uvZoom, MIN_ZOOM);
}

void updateUVEditor(AppContext& ctx) {
    WorkspaceState& workspace = ctx.workspace;
    const InputState& input = ctx.systems.input;
    const Rect area = screenLayout(ctx).uvEditor;
    const Vec2 mouse(static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()));

    // Only while nothing else owns the mouse: no open list from the header, console, or modal window
    const ContextManager& contexts = ctx.systems.input_ctx;
    const bool free = !ctx.ui.popupOpen() && !contexts.isActive(InputContext_Console) && !contexts.isActive(InputContext_Modal);
    const bool over = free && area.contains(mouse);

    // A drag started over the editor keeps going wherever the mouse goes
    const u16 middle = static_cast<u16>(MouseButton::Middle);
    const u16 right = static_cast<u16>(MouseButton::Right);
    if (over && (input.wasMousePressedThisFrame(middle) || input.wasMousePressedThisFrame(right))) workspace.uvPanning = true;
    if (!input.isMouseDown(middle) && !input.isMouseDown(right)) workspace.uvPanning = false;
    if (workspace.uvPanning && workspace.uvZoom > 0.0f) {
        workspace.uvCenter = workspace.uvCenter - Vec2(static_cast<f32>(input.getMouseDeltaX()), static_cast<f32>(input.getMouseDeltaY())) / workspace.uvZoom;
    }

    // Zooming keeps the UV under the cursor where it is
    const i32 scroll = input.getScroll();
    if (over && scroll != 0 && workspace.uvZoom > 0.0f) {
        const Vec2 under = screenToUV(workspace, area, mouse);
        workspace.uvZoom = std::clamp(workspace.uvZoom * std::pow(ZOOM_STEP, static_cast<f32>(scroll) / WHEEL_NOTCH), MIN_ZOOM, MAX_ZOOM);
        workspace.uvCenter = under - (mouse - area.center()) / workspace.uvZoom;
    }
}

u32 uvBackgroundTexture(const AppContext& ctx) {
    const WorkspaceState& workspace = ctx.workspace;
    if (workspace.uvBackground == UVBackground::Checker) return 0;
    if (workspace.uvBackground == UVBackground::Texture) {
        const Texture* texture = ctx.scene.textures.tryGet(workspace.uvTexture);
        return texture ? ctx.pictureTextures.find(texture->picture) : 0;
    }

    // From the material: the first selected face's own, otherwise the object's
    const Object* object = ctx.scene.objects.tryGet(uvObject(ctx));
    if (!object) return 0;
    const MaterialCollection& materials = ctx.scene.materials;
    MaterialHandle material = object->material;
    for (FaceHandle face : ctx.scene.selection.getFaceHandles()) {
        const MaterialHandle own = object->meshData.getFaceMaterial(face);
        if (materials.isValid(own)) material = own;
        break;
    }
    return mapTexture(ctx, materials.get(materials.resolve(material)));
}

void drawUVEditor(AppContext& ctx, const Rect& area) {
    WorkspaceState& workspace = ctx.workspace;
    if (workspace.uvZoom <= 0.0f) frameUVs(ctx, true);
    if (workspace.uvZoom <= 0.0f) workspace.uvZoom = std::max(MIN_ZOOM, std::min(area.width, area.height) * FRAME_MARGIN);

    UIDrawList& list = ctx.ui.drawList();
    list.rect(area, EDITOR_BACKGROUND);

    // The texture itself: the 0 to 1 square, with the picture or a checker
    const Vec2 squareTopLeft = uvToScreen(workspace, area, Vec2(0.0f, 0.0f));
    const Vec2 squareBottomRight = uvToScreen(workspace, area, Vec2(1.0f, 1.0f));
    const Rect square { squareTopLeft.x, squareTopLeft.y, squareBottomRight.x - squareTopLeft.x, squareBottomRight.y - squareTopLeft.y };
    const u32 background = uvBackgroundTexture(ctx);
    if (background != 0) {
        list.image(square, background);
    } else {
        const f32 cell = square.width / CHECKER_CELLS;
        for (u32 y = 0; y < CHECKER_CELLS; ++y) {
            for (u32 x = 0; x < CHECKER_CELLS; ++x) {
                list.rect({ square.x + x * cell, square.y + y * cell, cell + 0.5f, cell + 0.5f }, (x + y) % 2 == 0 ? CHECKER_LIGHT : CHECKER_DARK);
            }
        }
    }

    if (workspace.uvGrid) drawGrid(list, workspace, area);

    // Its edge, so the texture's bounds stay clear over any picture
    list.rect({ square.x, square.y, square.width, 1.0f }, SQUARE_BORDER);
    list.rect({ square.x, square.bottom() - 1.0f, square.width, 1.0f }, SQUARE_BORDER);
    list.rect({ square.x, square.y, 1.0f, square.height }, SQUARE_BORDER);
    list.rect({ square.right() - 1.0f, square.y, 1.0f, square.height }, SQUARE_BORDER);

    // Every face's UVs, as a closed outline
    const Object* object = ctx.scene.objects.tryGet(uvObject(ctx));
    if (!object) return;
    const MeshData& mesh = object->meshData;
    for (FaceHandle face : mesh.getFaceHandles()) {
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        for (std::size_t i = 0; i < uvs.size(); ++i) {
            list.line(uvToScreen(workspace, area, uvs[i]), uvToScreen(workspace, area, uvs[(i + 1) % uvs.size()]), WIRE_WIDTH, WIRE);
        }
    }
}

void frameUVs(AppContext& ctx, bool all) {
    const Rect area = screenLayout(ctx).uvEditor;
    Vec2 low, high;
    if (area.width <= 0.0f || area.height <= 0.0f || !uvBounds(ctx, all, low, high)) return;
    fitUVs(ctx.workspace, area, low, high);
}

void frameScene(AppContext& ctx, bool all) {
    const ObjectHandle handle = uvObject(ctx);
    if (!ctx.scene.objects.isValid(handle)) return;
    const MeshData& mesh = ctx.scene.objects.get(handle).meshData;
    const Mat4 world = ctx.scene.objects.worldMatrix(handle);
    const Selection& selection = ctx.scene.selection;

    // The selected vertices, or every vertex, in the world
    std::vector<Vec3> points;
    const auto add = [&](VertexHandle vertex) {
        const Vec3 p = mesh.getVertexPosition(vertex);
        const Vec4 w = world * Vec4(p.x, p.y, p.z, 1.0f);
        points.push_back(Vec3(w.x, w.y, w.z));
    };
    if (!all) {
        for (VertexHandle vertex : mesh.getVertexHandles()) if (selection.hasVertex(handle, vertex)) add(vertex);
    }
    if (points.empty()) for (VertexHandle vertex : mesh.getVertexHandles()) add(vertex);
    if (points.empty()) return;

    // A sphere around them, seen whole at the narrower of the view's two angles
    Vec3 low = points.front(), high = points.front();
    for (const Vec3& p : points) {
        low = Vec3(std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z));
        high = Vec3(std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z));
    }
    const Vec3 center = (low + high) * 0.5f;
    f32 radius = 0.05f;
    for (const Vec3& p : points) radius = std::max(radius, (p - center).length());

    Camera& camera = ctx.scene.camera;
    const Rect view = sceneView(ctx);
    const f32 aspect = view.height > 0.0f ? view.width / view.height : 1.0f;
    const f32 halfVertical = camera.fovRadians * 0.5f;
    const f32 halfHorizontal = std::atan(std::tan(halfVertical) * aspect);
    camera.target = center;
    camera.distance = radius / std::sin(std::min(halfVertical, halfHorizontal)) / FRAME_MARGIN;
    camera.updatePositionFromOrbit();
}

void frameView(AppContext& ctx, bool all) {
    const Vec2 mouse(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    if (screenLayout(ctx).uvEditor.contains(mouse)) frameUVs(ctx, all);
    else frameScene(ctx, all);
}
