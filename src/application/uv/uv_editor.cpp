#include "application/uv/uv_editor.hpp"
#include "application/viewport/material_view.hpp"
#include "application/paint/paint_workspace.hpp"
#include "scene/selection/mesh_selection.hpp"
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
    const Color WIRE { 0.90f, 0.91f, 0.95f, 0.55f };
    const Color SELECTED { 0.74f, 0.58f, 0.98f, 1.0f };
    const Color SELECTED_FILL { 0.74f, 0.58f, 0.98f, 0.28f };
    const Color POINT { 0.90f, 0.91f, 0.95f, 0.8f };
    const Color GUIDE { 0.95f, 0.95f, 0.98f, 0.7f };
    const Color SEAM { 0.31f, 0.98f, 0.48f, 1.0f };
    constexpr f32 SEAM_WIDTH = 2.0f;
    constexpr f32 SELECTED_WIRE_WIDTH = 2.5f;
    constexpr f32 POINT_SIZE = 5.0f;
    constexpr f32 SELECTED_POINT_SIZE = 7.0f;
    constexpr f32 PICK_RADIUS = 10.0f;      // pixels

    f32 distanceToSegment(Vec2 point, Vec2 a, Vec2 b) {
        const Vec2 ab = b - a;
        const f32 lengthSq = Vec2::dot(ab, ab);
        const f32 t = lengthSq > 0.0f ? std::clamp(Vec2::dot(point - a, ab) / lengthSq, 0.0f, 1.0f) : 0.0f;
        return (point - (a + ab * t)).length();
    }

    // Even-odd test against the face's UV outline
    bool insideUVs(const std::vector<Vec2>& uvs, Vec2 point) {
        bool inside = false;
        for (std::size_t i = 0, j = uvs.size() - 1; i < uvs.size(); j = i++) {
            const Vec2& a = uvs[i];
            const Vec2& b = uvs[j];
            if ((a.y > point.y) != (b.y > point.y) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
        }
        return inside;
    }

    // A click in the editor, in the current mode: the nearest corner's vertex, the nearest edge, or the face under it
    // (with its whole island in island mode). Shift adds or takes away; a click on nothing clears unless Shift is held.
    void clickUVs(AppContext& ctx, const Rect& area, Vec2 mouse, bool toggling) {
        const ObjectHandle handle = uvObject(ctx);
        Object* object = ctx.scene.objects.tryGet(handle);
        if (!object) return;
        const MeshData& mesh = object->meshData;
        Selection& selection = ctx.scene.selection;
        const WorkspaceState& workspace = ctx.workspace;
        const u32 mode = ctx.systems.input_ctx.getSelectionContext();

        if (!toggling) selection.clearMeshElements();

        if (mode == InputContext_SelectionVertex) {
            VertexHandle best = INVALID_VERTEX;
            f32 bestDistance = PICK_RADIUS;
            for (FaceHandle face : mesh.getFaceHandles()) {
                const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
                const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
                for (std::size_t i = 0; i < vertices.size() && i < uvs.size(); ++i) {
                    const f32 distance = (uvToScreen(workspace.uvView, area, uvs[i]) - mouse).length();
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = vertices[i];
                    }
                }
            }
            if (best.isNull()) return;
            if (toggling && selection.hasVertex(handle, best)) selection.removeVertex(handle, best);
            else selection.addVertex(handle, best);
        } else if (mode == InputContext_SelectionEdge) {
            // A face's half-edges run in the same order as its UVs: half-edge i ends at corner i
            EdgeHandle best = INVALID_EDGE;
            f32 bestDistance = PICK_RADIUS;
            for (FaceHandle face : mesh.getFaceHandles()) {
                const std::vector<EdgeHandle> edges = mesh.getLoopEdges(mesh.getFace(face)->edge);
                const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
                for (std::size_t i = 0; i < edges.size() && i < uvs.size(); ++i) {
                    const Vec2 from = uvToScreen(workspace.uvView, area, uvs[(i + uvs.size() - 1) % uvs.size()]);
                    const Vec2 to = uvToScreen(workspace.uvView, area, uvs[i]);
                    const f32 distance = distanceToSegment(mouse, from, to);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = edges[i];
                    }
                }
            }
            if (best.isNull()) return;
            if (toggling && isEdgeSelected(selection, handle, mesh, best)) deselectEdge(selection, handle, mesh, best);
            else selectEdge(selection, handle, mesh, best);
        } else if (mode == InputContext_SelectionFace) {
            // The last face drawn there is the one on top
            const Vec2 point = screenToUV(workspace.uvView, area, mouse);
            FaceHandle hit = INVALID_FACE;
            for (FaceHandle face : mesh.getFaceHandles()) {
                if (insideUVs(mesh.getFaceUVs(face), point)) hit = face;
            }
            if (hit.isNull()) return;

            const std::vector<FaceHandle> faces = workspace.uvIslands ? mesh.getUVIsland(hit) : std::vector<FaceHandle> { hit };
            const bool deselecting = toggling && selection.hasFace(handle, hit);
            for (FaceHandle face : faces) {
                if (deselecting) deselectFace(selection, handle, mesh, face);
                else selectFace(selection, handle, mesh, face);
            }
        }
    }

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

    // Grid lines across the visible part of the editor, every step in UV units; whole units stronger
    void drawGrid(UIDrawList& list, const UVView& view, const Rect& area) {
        if (view.zoom * GRID_STEP < MIN_GRID_SPACING) return;
        const Vec2 topLeft = screenToUV(view, area, Vec2(area.x, area.y));
        const Vec2 bottomRight = screenToUV(view, area, Vec2(area.right(), area.bottom()));

        for (f32 u = std::floor(topLeft.x / GRID_STEP) * GRID_STEP; u <= bottomRight.x; u += GRID_STEP) {
            const f32 x = uvToScreen(view, area, Vec2(u, 0.0f)).x;
            const bool unit = std::fabs(u - std::round(u)) < 1e-4f;
            list.rect({ std::floor(x), area.y, 1.0f, area.height }, unit ? UNIT_LINE : GRID_LINE);
        }
        for (f32 v = std::floor(topLeft.y / GRID_STEP) * GRID_STEP; v <= bottomRight.y; v += GRID_STEP) {
            const f32 y = uvToScreen(view, area, Vec2(0.0f, v)).y;
            const bool unit = std::fabs(v - std::round(v)) < 1e-4f;
            list.rect({ area.x, std::floor(y), area.width, 1.0f }, unit ? UNIT_LINE : GRID_LINE);
        }
    }
}

void drawTextureSquare(UIDrawList& list, const UVView& view, const Rect& area, u32 texture, bool grid) {
    list.rect(area, EDITOR_BACKGROUND);

    // The texture itself: the 0 to 1 square, with the picture or a checker
    const Vec2 squareTopLeft = uvToScreen(view, area, Vec2(0.0f, 0.0f));
    const Vec2 squareBottomRight = uvToScreen(view, area, Vec2(1.0f, 1.0f));
    const Rect square { squareTopLeft.x, squareTopLeft.y, squareBottomRight.x - squareTopLeft.x, squareBottomRight.y - squareTopLeft.y };
    if (texture != 0) {
        list.image(square, texture);
    } else {
        const f32 cell = square.width / CHECKER_CELLS;
        for (u32 y = 0; y < CHECKER_CELLS; ++y) {
            for (u32 x = 0; x < CHECKER_CELLS; ++x) {
                list.rect({ square.x + x * cell, square.y + y * cell, cell + 0.5f, cell + 0.5f }, (x + y) % 2 == 0 ? CHECKER_LIGHT : CHECKER_DARK);
            }
        }
    }

    if (grid) drawGrid(list, view, area);

    // Its edge, so the texture's bounds stay clear over any picture
    list.rect({ square.x, square.y, square.width, 1.0f }, SQUARE_BORDER);
    list.rect({ square.x, square.bottom() - 1.0f, square.width, 1.0f }, SQUARE_BORDER);
    list.rect({ square.x, square.y, 1.0f, square.height }, SQUARE_BORDER);
    list.rect({ square.right() - 1.0f, square.y, 1.0f, square.height }, SQUARE_BORDER);
}

Vec2 uvToScreen(const UVView& view, const Rect& area, Vec2 uv) {
    return area.center() + (uv - view.center) * view.zoom;
}

Vec2 screenToUV(const UVView& view, const Rect& area, Vec2 screen) {
    return view.center + (screen - area.center()) / std::max(view.zoom, MIN_ZOOM);
}

bool navigateUVView(AppContext& ctx, UVView& view, const Rect& area) {
    const InputState& input = ctx.systems.input;
    const Vec2 mouse(static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()));

    // Only while nothing else owns the mouse: no open list from a header, console, or modal window
    const ContextManager& contexts = ctx.systems.input_ctx;
    const bool free = !ctx.ui.popupOpen() && !contexts.isActive(InputContext_Console) && !contexts.isActive(InputContext_Modal);
    const bool over = free && area.contains(mouse);

    // A drag started over the view keeps going wherever the mouse goes
    const u16 middle = static_cast<u16>(MouseButton::Middle);
    const u16 right = static_cast<u16>(MouseButton::Right);
    if (over && (input.wasMousePressedThisFrame(middle) || input.wasMousePressedThisFrame(right))) view.panning = true;
    if (!input.isMouseDown(middle) && !input.isMouseDown(right)) view.panning = false;
    if (view.panning && view.zoom > 0.0f) {
        view.center = view.center - Vec2(static_cast<f32>(input.getMouseDeltaX()), static_cast<f32>(input.getMouseDeltaY())) / view.zoom;
    }

    // Zooming keeps the UV under the cursor where it is
    const i32 scroll = input.getScroll();
    if (over && scroll != 0 && view.zoom > 0.0f) {
        const Vec2 under = screenToUV(view, area, mouse);
        view.zoom = std::clamp(view.zoom * std::pow(ZOOM_STEP, static_cast<f32>(scroll) / WHEEL_NOTCH), MIN_ZOOM, MAX_ZOOM);
        view.center = under - (mouse - area.center()) / view.zoom;
    }
    return over;
}

void fitUVView(UVView& view, const Rect& area, Vec2 low, Vec2 high) {
    const f32 width = std::max(high.x - low.x, MIN_FRAME_SIZE);
    const f32 height = std::max(high.y - low.y, MIN_FRAME_SIZE);
    view.center = (low + high) * 0.5f;
    view.zoom = std::clamp(std::min(area.width / width, area.height / height) * FRAME_MARGIN, MIN_ZOOM, MAX_ZOOM);
}

void updateUVEditor(AppContext& ctx) {
    UVView& view = ctx.workspace.uvView;
    const Rect area = screenLayout(ctx).uvEditor;
    const bool over = navigateUVView(ctx, view, area);

    // A left click picks in the current mode
    const InputState& input = ctx.systems.input;
    if (over && input.wasMousePressedThisFrame(static_cast<u16>(MouseButton::Left)) && view.zoom > 0.0f) {
        const Vec2 mouse(static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()));
        const bool toggling = ctx.systems.actions.isActionDown(Action::ToggleSelection, input, ctx.systems.input_ctx.getContext());
        clickUVs(ctx, area, mouse, toggling);
    }
}

u32 uvBackgroundTexture(const AppContext& ctx) {
    const WorkspaceState& workspace = ctx.workspace;
    if (workspace.uvBackground == UVBackground::Checker) return 0;
    if (workspace.uvBackground == UVBackground::Texture) {
        return textureImage(ctx, workspace.uvTexture);
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
    if (workspace.uvView.zoom <= 0.0f) frameUVs(ctx, true);
    if (workspace.uvView.zoom <= 0.0f) workspace.uvView.zoom = std::max(MIN_ZOOM, std::min(area.width, area.height) * FRAME_MARGIN);

    UIDrawList& list = ctx.ui.drawList();
    drawTextureSquare(list, workspace.uvView, area, uvBackgroundTexture(ctx), workspace.uvGrid);

    const ObjectHandle handle = uvObject(ctx);
    const Object* object = ctx.scene.objects.tryGet(handle);
    if (!object) return;
    const MeshData& mesh = object->meshData;
    const Selection& selection = ctx.scene.selection;
    const u32 mode = ctx.systems.input_ctx.getSelectionContext();

    // Selected faces filled first, so every outline draws over them
    if (mode == InputContext_SelectionFace) {
        for (FaceHandle face : selection.getFaceHandles()) {
            const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
            const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
            const auto uvOf = [&](VertexHandle vertex) {
                for (std::size_t i = 0; i < vertices.size() && i < uvs.size(); ++i) if (vertices[i] == vertex) return uvs[i];
                return Vec2();
            };
            for (const Triangle& triangle : mesh.getFaceTriangles(face)) {
                list.triangle(uvToScreen(workspace.uvView, area, uvOf(triangle.v0)), uvToScreen(workspace.uvView, area, uvOf(triangle.v1)),
                              uvToScreen(workspace.uvView, area, uvOf(triangle.v2)), SELECTED_FILL);
            }
        }
    }

    // Every face's UVs as an outline, dim; the selection's edges bright on top: edges with both ends selected
    // (vertex mode), selected edges, or selected faces' edges
    std::vector<std::pair<Vec2, Vec2>> selectedLines;
    for (FaceHandle face : mesh.getFaceHandles()) {
        const std::vector<EdgeHandle> edges = mesh.getLoopEdges(mesh.getFace(face)->edge);
        const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
        const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
        const bool faceSelected = mode == InputContext_SelectionFace && selection.hasFace(handle, face);

        for (std::size_t i = 0; i < uvs.size(); ++i) {
            const std::size_t previous = (i + uvs.size() - 1) % uvs.size();
            const Vec2 from = uvToScreen(workspace.uvView, area, uvs[previous]);
            const Vec2 to = uvToScreen(workspace.uvView, area, uvs[i]);

            bool selected = faceSelected;
            if (mode == InputContext_SelectionVertex && i < vertices.size()) {
                selected = selection.hasVertex(handle, vertices[previous]) && selection.hasVertex(handle, vertices[i]);
            } else if (mode == InputContext_SelectionEdge && i < edges.size()) {
                selected = isEdgeSelected(selection, handle, mesh, edges[i]);
            }

            if (selected) selectedLines.push_back({ from, to });
            else if (i < edges.size() && mesh.isSeam(edges[i])) list.line(from, to, SEAM_WIDTH, SEAM);
            else list.line(from, to, WIRE_WIDTH, WIRE);
        }
    }
    for (const auto& [from, to] : selectedLines) list.line(from, to, SELECTED_WIRE_WIDTH, SELECTED);

    // Scaling and rotating measure from the pivot: a line from it to the mouse
    if (ctx.uvTool.active() && ctx.uvTool.kind != UVToolKind::Grab) {
        const Vec2 pivot = uvToScreen(workspace.uvView, area, ctx.uvTool.pivot);
        const Vec2 mouse(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
        list.line(pivot, mouse, 1.0f, GUIDE);
        list.roundedRect({ pivot.x - 3.0f, pivot.y - 3.0f, 6.0f, 6.0f }, 3.0f, GUIDE);
    }

    // Vertex mode: a dot at every corner, the selected ones bigger and purple
    if (mode == InputContext_SelectionVertex) {
        for (FaceHandle face : mesh.getFaceHandles()) {
            const std::vector<VertexHandle> vertices = mesh.getFaceVertices(face);
            const std::vector<Vec2> uvs = mesh.getFaceUVs(face);
            for (std::size_t i = 0; i < vertices.size() && i < uvs.size(); ++i) {
                const bool selected = selection.hasVertex(handle, vertices[i]);
                const f32 size = selected ? SELECTED_POINT_SIZE : POINT_SIZE;
                const Vec2 at = uvToScreen(workspace.uvView, area, uvs[i]);
                list.roundedRect({ at.x - size * 0.5f, at.y - size * 0.5f, size, size }, size * 0.5f, selected ? SELECTED : POINT);
            }
        }
    }
}

void selectAllUVs(AppContext& ctx) {
    const ObjectHandle handle = uvObject(ctx);
    const Object* object = ctx.scene.objects.tryGet(handle);
    if (!object) return;

    const u32 mode = ctx.systems.input_ctx.getSelectionContext();
    const SelectAllMode which = mode == InputContext_SelectionEdge ? SelectAllMode::Edges
                              : mode == InputContext_SelectionFace ? SelectAllMode::Faces : SelectAllMode::Vertices;
    selectAll(ctx.scene.selection, handle, object->meshData, which);
}

void frameUVs(AppContext& ctx, bool all) {
    const Rect area = screenLayout(ctx).uvEditor;
    Vec2 low, high;
    if (area.width <= 0.0f || area.height <= 0.0f || !uvBounds(ctx, all, low, high)) return;
    fitUVView(ctx.workspace.uvView, area, low, high);
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
    // Paint has one view: the flat texture, or the whole model
    if (ctx.workspace.current == Workspace::Paint) {
        if (ctx.workspace.paint2D) framePaintCanvas(ctx, all);
        else frameScene(ctx, true);
        return;
    }
    const Vec2 mouse(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    if (screenLayout(ctx).uvEditor.contains(mouse)) frameUVs(ctx, all);
    else frameScene(ctx, all);
}
