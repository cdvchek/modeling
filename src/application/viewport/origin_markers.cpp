#include "application/viewport/origin_markers.hpp"
#include "application/workspace.hpp"
#include "core/math/projection.hpp"
#include "core/math/vec4.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>

namespace {
    constexpr f32 DOT_RADIUS = 4.0f;
    constexpr f32 SELECTED_DOT_RADIUS = 5.0f;
    constexpr f32 OUTLINE_WIDTH = 1.5f;
    constexpr f32 AXIS_LENGTH = 34.0f;   // pixels
    constexpr f32 AXIS_WIDTH = 2.0f;
    constexpr f32 AXIS_HALO_WIDTH = 4.0f;
    constexpr f32 DASH_LENGTH = 6.0f;
    constexpr f32 DASH_GAP = 5.0f;

    const Color OUTLINE { 0.06f, 0.06f, 0.08f, 0.9f };
    const Color ACTIVE_COLOR = UIStyle::TEXT;
    const Color GREYED_COLOR { 0.38f, 0.45f, 0.64f, 0.75f };
    const Color AXIS_HALO { 0.06f, 0.06f, 0.08f, 0.6f };

    Rect circle(Vec2 center, f32 radius) {
        return { center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f };
    }

    // The object's own axes in the world: its rotation, without scale
    Vec3 axisDirection(const Transform& transform, Vec3 axis) {
        Transform rotationOnly;
        rotationOnly.rotation = transform.rotation;
        const Vec4 direction = rotationOnly.getMatrix() * Vec4(axis.x, axis.y, axis.z, 0.0f);
        return Vec3(direction.x, direction.y, direction.z).normalized();
    }

    // Lines from the dot along +X, +Y, +Z, a fixed length on screen whatever the distance
    void drawAxes(UIDrawList& ui, const Transform& transform, Vec2 dot, const Camera& camera, const Mat4& viewProjection, const Rect& view) {
        const f32 distance = (transform.position - camera.position).length();
        const f32 unitsPerPixel = distance * 2.0f * std::tan(camera.fovRadians * 0.5f) / view.height;

        const Vec3 axes[3] = { Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1) };
        const Color colors[3] = { UIStyle::AXIS_X, UIStyle::AXIS_Y, UIStyle::AXIS_Z };

        Vec2 ends[3];
        bool visible[3];
        for (int i = 0; i < 3; ++i) {
            const Vec3 tip = transform.position + axisDirection(transform, axes[i]) * (AXIS_LENGTH * unitsPerPixel);
            visible[i] = projectToView(viewProjection, tip, view, ends[i]);
        }

        // Halos first so no axis draws over another's color
        for (int i = 0; i < 3; ++i) if (visible[i]) ui.line(dot, ends[i], AXIS_HALO_WIDTH, AXIS_HALO);
        for (int i = 0; i < 3; ++i) if (visible[i]) ui.line(dot, ends[i], AXIS_WIDTH, colors[i]);
    }
}

void drawParentLines(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, const Rect& view) {
    if (ctx.systems.input_ctx.getSelectionContext() != InputContext_SelectionObject) return;

    const ObjectCollection& objects = ctx.scene.objects;
    for (ObjectHandle handle : objects.handles()) {
        const ObjectHandle parent = objects.parentOf(handle);
        if (parent.isNull()) continue;

        Vec2 from, to;
        if (!projectToView(viewProjection, objects.worldTransform(handle).position, view, from)) continue;
        if (!projectToView(viewProjection, objects.worldTransform(parent).position, view, to)) continue;

        const Vec2 delta = to - from;
        const f32 length = delta.length();
        if (length < 1.0f) continue;

        const Vec2 direction = delta / length;
        for (f32 start = 0.0f; start < length; start += DASH_LENGTH + DASH_GAP) {
            const f32 end = std::min(start + DASH_LENGTH, length);
            ui.line(from + direction * start, from + direction * end, 1.0f, UIStyle::GUIDE_LINE);
        }
    }
}

void drawOriginMarkers(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, const Rect& view) {
    if (!ctx.viewport.showOrigins) return;

    const Selection& selection = ctx.scene.selection;
    const ObjectHandle active = selection.getActiveObject();
    const ObjectHandle selected = selection.getOrigin();

    // Greyed ones first, so the active and selected origins sit on top
    for (int pass = 0; pass < 2; ++pass) {
        for (ObjectHandle handle : ctx.scene.objects.handles()) {
            const bool highlighted = handle == active || handle == selected;
            if (highlighted != (pass == 1)) continue;

            const Transform transform = ctx.scene.objects.worldTransform(handle);
            Vec2 dot;
            if (!projectToView(viewProjection, transform.position, view, dot)) continue;

            if (handle == selected) {
                drawAxes(ui, transform, dot, ctx.scene.camera, viewProjection, view);
                ui.roundedRect(circle(dot, SELECTED_DOT_RADIUS), SELECTED_DOT_RADIUS, UIStyle::ACCENT, OUTLINE, OUTLINE_WIDTH);
            } else {
                ui.roundedRect(circle(dot, DOT_RADIUS), DOT_RADIUS, handle == active ? ACTIVE_COLOR : GREYED_COLOR, OUTLINE, OUTLINE_WIDTH);
            }
        }
    }
}
