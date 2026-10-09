#include "application/tools/tool_guides.hpp"
#include "core/math/projection.hpp"
#include "core/math/screen_drag.hpp"
#include "core/math/vec4.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>

namespace {
    constexpr f32 DOT_RADIUS = 2.5f;
    constexpr f32 LINE_WIDTH = 1.0f;
    constexpr f32 HALO_WIDTH = 1.0f;
    // Keeps the scale factor sane when the tool starts with the mouse right on the pivot
    constexpr f32 MIN_START_DISTANCE = 1.0f;

    Vec2 mousePosition(const AppContext& ctx) {
        return Vec2(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    }

    // Center of the selection in world space: the start positions of the vertices or objects, or the selected lights
    Vec3 selectionCenter(const AppContext& ctx) {
        const Selection& selection = ctx.scene.selection;
        const std::vector<Vec3>& starts = selection.getSelectionStartPositions();
        const std::vector<Transform>& objectStarts = selection.getObjectStartTransforms();

        if (selection.hasOrigin()) return ctx.originEdit.start.world.position;

        const std::vector<Transform>& referenceStarts = selection.getReferenceStartTransforms();
        if (selection.hasReferences() && !referenceStarts.empty()) {
            Vec3 center(0.0f);
            for (const Transform& start : referenceStarts) center += start.position;
            return center / static_cast<f32>(referenceStarts.size());
        }

        if (!objectStarts.empty()) {
            Vec3 center(0.0f);
            for (const Transform& start : objectStarts) center += start.position;
            return center / static_cast<f32>(objectStarts.size());
        }

        if (!selection.getVertices().empty() && !starts.empty()) {
            Vec3 center(0.0f);
            for (const Vec3& start : starts) center += start;
            center = center / static_cast<f32>(starts.size());

            const ObjectHandle object = selection.getVertices()[0].object;
            if (!ctx.scene.objects.isValid(object)) return center;

            const Vec4 world = ctx.scene.objects.worldMatrix(object) * Vec4(center.x, center.y, center.z, 1.0f);
            return Vec3(world.x, world.y, world.z);
        }

        Vec3 center(0.0f);
        u32 count = 0;
        for (LightHandle handle : selection.getLights()) {
            if (const Light* light = ctx.scene.lights.tryGet(handle)) {
                center += light->position;
                ++count;
            }
        }
        return count > 0 ? center / static_cast<f32>(count) : center;
    }

    void dot(UIDrawList& ui, Vec2 center, f32 radius, Color color) {
        ui.roundedRect({ center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f }, radius, color);
    }

    // A dark halo goes under the line and dot first
    void guide(UIDrawList& ui, Vec2 from, Vec2 to) {
        const bool hasLine = (to - from).length() > DOT_RADIUS;

        if (hasLine) ui.line(from, to, LINE_WIDTH + HALO_WIDTH * 2.0f, UIStyle::GUIDE_HALO);
        dot(ui, from, DOT_RADIUS + HALO_WIDTH, UIStyle::GUIDE_HALO);

        if (hasLine) ui.line(from, to, LINE_WIDTH, UIStyle::GUIDE_LINE);
        dot(ui, from, DOT_RADIUS, UIStyle::GUIDE_DOT);
    }
}

bool initTransformTool(AppContext& ctx) {
    TransformTool& tool = ctx.transformTool;
    if (tool.initialized) return true;

    u32 width = 0;
    u32 height = 0;
    ctx.windows[0]->getDimensions(width, height);
    if (width == 0 || height == 0) return false;

    const Camera& camera = ctx.scene.camera;
    const Mat4 viewProjection = camera.getProjectionMatrix(static_cast<f32>(width) / static_cast<f32>(height)) * camera.getViewMatrix();

    if (!projectToScreen(viewProjection, selectionCenter(ctx), static_cast<f32>(width), static_cast<f32>(height), tool.pivot)) return false;

    const Vec2 mouse = mousePosition(ctx);
    tool.startDistance = std::max((mouse - tool.pivot).length(), MIN_START_DISTANCE);
    tool.lastAngle = screenAngle(tool.pivot, mouse);
    tool.angle = 0.0f;
    tool.initialized = true;
    return true;
}

void drawToolGuides(const AppContext& ctx, UIDrawList& ui) {
    const ContextManager& contexts = ctx.systems.input_ctx;
    const Vec2 mouse = mousePosition(ctx);

    if (contexts.isActive(InputContext_Scale | InputContext_Rotate) && ctx.transformTool.initialized) {
        guide(ui, ctx.transformTool.pivot, mouse);
    } else if (contexts.isActive(InputContext_Bevel | InputContext_Inset)) {
        guide(ui, Vec2(ctx.widthTool.startMouseX, ctx.widthTool.startMouseY), mouse);
    }
}
