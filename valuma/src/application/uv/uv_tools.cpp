#include "application/uv/uv_tools.hpp"
#include "application/uv/uv_editor.hpp"
#include "application/app_context.hpp"
#include "core/math/screen_drag.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
    // Scale measures from at least this far from the pivot, so starting right on it doesn't jump
    constexpr f32 MIN_START_DISTANCE = 20.0f;

    Vec2 mousePosition(const AppContext& ctx) {
        return Vec2(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    }

    bool keyPressed(const AppContext& ctx, Key key) {
        return ctx.systems.input.wasKeyPressedThisFrame(static_cast<u16>(key));
    }

    void finish(AppContext& ctx, bool keep) {
        if (keep) ctx.history.commit();
        else ctx.history.cancel(ctx.scene);
        ctx.uvTool = {};
    }

    // Where the pivot sits in the editor, in window pixels
    Vec2 pivotOnScreen(const AppContext& ctx) {
        return uvToScreen(ctx.workspace.uvView, screenLayout(ctx).uvEditor, ctx.uvTool.pivot);
    }
}

std::vector<u32> uvToolCorners(const AppContext& ctx) {
    std::vector<u32> corners;
    const ObjectHandle handle = uvObject(ctx);
    const Object* object = ctx.scene.objects.tryGet(handle);
    if (!object) return corners;
    const MeshData& mesh = object->meshData;
    const Selection& selection = ctx.scene.selection;
    const bool faces = ctx.systems.input_ctx.getModeContext() == InputContext_SelectionFace;

    // A half-edge holds the UV of its face's corner at its tip; its place in getEdgeHandles is its place in the list
    const std::vector<EdgeHandle> edges = mesh.getEdgeHandles();
    for (u32 i = 0; i < edges.size(); ++i) {
        const Edge* edge = mesh.getEdge(edges[i]);
        if (!edge || edge->face.isNull()) continue;
        const bool moves = faces ? selection.hasFace(handle, edge->face) : selection.hasVertex(handle, edge->tip);
        if (moves) corners.push_back(i);
    }
    return corners;
}

bool canStartUVTool(const AppContext& ctx) {
    return !ctx.uvTool.active() && !uvToolCorners(ctx).empty();
}

void startUVTool(AppContext& ctx, UVToolKind kind) {
    const ObjectHandle handle = uvObject(ctx);
    const Object* object = ctx.scene.objects.tryGet(handle);
    if (!object || kind == UVToolKind::None) return;

    UVToolState tool;
    tool.kind = kind;
    tool.object = handle;
    tool.startUVs = object->meshData.getCornerUVs();
    tool.moving = uvToolCorners(ctx);
    if (tool.moving.empty()) return;

    // Scale and rotate turn around the middle of what moves
    Vec2 low(FLT_MAX, FLT_MAX), high(-FLT_MAX, -FLT_MAX);
    for (u32 index : tool.moving) {
        const Vec2& uv = tool.startUVs[index];
        low = Vec2(std::min(low.x, uv.x), std::min(low.y, uv.y));
        high = Vec2(std::max(high.x, uv.x), std::max(high.y, uv.y));
    }
    tool.pivot = (low + high) * 0.5f;
    tool.startMouse = mousePosition(ctx);

    ctx.uvTool = tool;
    ctx.uvTool.lastAngle = screenAngle(pivotOnScreen(ctx), tool.startMouse);
    ctx.history.begin(ctx.scene);
}

void updateUVTool(AppContext& ctx) {
    UVToolState& tool = ctx.uvTool;
    Object* object = ctx.scene.objects.tryGet(tool.object);
    if (!object || tool.startUVs.size() != object->meshData.getEdgeHandles().size()) {
        finish(ctx, false);
        return;
    }

    const InputState& input = ctx.systems.input;
    if (keyPressed(ctx, Key::Escape) || input.wasMousePressedThisFrame(static_cast<u16>(MouseButton::Right))) {
        finish(ctx, false);
        return;
    }

    // Axis locks: pressing the locked axis again frees it (rotating has nothing to lock)
    if (tool.kind != UVToolKind::Rotate) {
        if (keyPressed(ctx, Key::X)) tool.axis = tool.axis == UVAxis::X ? UVAxis::Free : UVAxis::X;
        if (keyPressed(ctx, Key::Y)) tool.axis = tool.axis == UVAxis::Y ? UVAxis::Free : UVAxis::Y;
    }

    // Where the corners are now, worked out from where they started so a long drag never drifts
    const Vec2 mouse = mousePosition(ctx);
    const Vec2 pivot = pivotOnScreen(ctx);
    const f32 zoom = std::max(ctx.workspace.uvView.zoom, 1.0f);
    std::vector<Vec2> uvs = tool.startUVs;

    if (tool.kind == UVToolKind::Grab) {
        Vec2 delta = (mouse - tool.startMouse) / zoom;
        if (tool.axis == UVAxis::X) delta.y = 0.0f;
        if (tool.axis == UVAxis::Y) delta.x = 0.0f;
        for (u32 index : tool.moving) uvs[index] = tool.startUVs[index] + delta;
    } else if (tool.kind == UVToolKind::Scale) {
        const f32 startDistance = std::max((tool.startMouse - pivot).length(), MIN_START_DISTANCE);
        const f32 factor = (mouse - pivot).length() / startDistance;
        const Vec2 scale(tool.axis == UVAxis::Y ? 1.0f : factor, tool.axis == UVAxis::X ? 1.0f : factor);
        for (u32 index : tool.moving) {
            const Vec2 offset = tool.startUVs[index] - tool.pivot;
            uvs[index] = tool.pivot + Vec2(offset.x * scale.x, offset.y * scale.y);
        }
    } else if (tool.kind == UVToolKind::Rotate) {
        // Swept angle adds up, so circling more than once keeps turning
        const f32 mouseAngle = screenAngle(pivot, mouse);
        tool.angle += wrapAngle(mouseAngle - tool.lastAngle);
        tool.lastAngle = mouseAngle;

        // UV space runs v down like the screen, so the same angle turns the same way the mouse does
        const f32 c = std::cos(tool.angle);
        const f32 s = std::sin(tool.angle);
        for (u32 index : tool.moving) {
            const Vec2 offset = tool.startUVs[index] - tool.pivot;
            uvs[index] = tool.pivot + Vec2(offset.x * c + offset.y * s, -offset.x * s + offset.y * c);
        }
    }

    object->meshData.setCornerUVs(uvs);
    object->meshDirty = true;

    if (keyPressed(ctx, Key::Enter) || input.wasMousePressedThisFrame(static_cast<u16>(MouseButton::Left))) finish(ctx, true);
}

const char* uvToolName(UVToolKind kind) {
    switch (kind) {
        case UVToolKind::Grab: return "Grab";
        case UVToolKind::Scale: return "Scale";
        case UVToolKind::Rotate: return "Rotate";
        default: return "Select";
    }
}
