#include "application/light_markers.hpp"
#include "ui/ui_style.hpp"
#include "core/math/projection.hpp"

#include <algorithm>
#include <cmath>

namespace {
    constexpr f32 ORB_RADIUS = 7.0f;
    constexpr f32 ORB_OUTLINE = 1.5f;
    constexpr f32 GLOW_BLUR = 10.0f;
    constexpr f32 GLOW_ALPHA = 0.35f;
    constexpr f32 DISABLED_RING = 2.0f;

    constexpr f32 DIRECTION_GAP = 4.0f;
    constexpr f32 DIRECTION_LENGTH = 36.0f;
    constexpr f32 DIRECTION_WIDTH = 2.0f;
    constexpr f32 ARROW_SIZE = 6.0f;

    // Directional lights get two extra arrows beside the main one
    constexpr f32 SIDE_ARROW_OFFSET = 8.0f;
    constexpr f32 SIDE_ARROW_LENGTH = 25.0f;
    constexpr f32 SIDE_ARROW_SIZE = 5.0f;

    constexpr f32 GROUND_DOT_RADIUS = 2.5f;

    constexpr f32 SELECTED_RING_GAP = 0.5f;
    constexpr f32 SELECTED_RING_WIDTH = 2.0f;
    constexpr f32 SELECTED_GLOW_BLUR = 7.0f;

    const Color OUTLINE_COLOR { 0.06f, 0.06f, 0.08f, 0.9f };
    const Color DISABLED_COLOR { 0.55f, 0.55f, 0.58f, 0.9f };
    const Color SELECTED_COLOR { 1.0f, 0.76f, 0.30f, 1.0f };
    const Color SELECTED_GLOW_COLOR { 1.0f, 0.70f, 0.25f, 0.45f };

    Color lightColor(const Light& light, f32 alpha) {
        return { light.color.x, light.color.y, light.color.z, alpha };
    }

    Rect circle(Vec2 center, f32 radius) {
        return { center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f };
    }

    // Faint line from the light down to the grid plane, with a dot where it lands
    void drawGroundLine(UIDrawList& ui, const Light& light, Vec2 orb, const Mat4& viewProjection, f32 width, f32 height) {
        if (light.position.y == 0.0f) return;

        Vec2 ground;
        if (!projectToScreen(viewProjection, Vec3(light.position.x, 0.0f, light.position.z), width, height, ground)) return;

        // Start at the orb's edge so the line doesn't show through a disabled light's hollow ring
        const Vec2 delta = ground - orb;
        if (delta.length() > ORB_RADIUS) {
            ui.line(orb + delta.normalized() * ORB_RADIUS, ground, 1.0f, UIStyle::GUIDE_LINE);
        }
        ui.roundedRect(circle(ground, GROUND_DOT_RADIUS), GROUND_DOT_RADIUS, UIStyle::GUIDE_DOT);
    }

    // Projects a world-space segment and draws it; skipped if either end is behind the camera
    struct WorldLines {
        UIDrawList& ui;
        const Mat4& viewProjection;
        f32 width;
        f32 height;

        void line(Vec3 a, Vec3 b, Color color) const {
            Vec2 screenA;
            Vec2 screenB;
            if (!projectToScreen(viewProjection, a, width, height, screenA)) return;
            if (!projectToScreen(viewProjection, b, width, height, screenB)) return;
            ui.line(screenA, screenB, DIRECTION_WIDTH, color);
        }
    };

    // A 3D arrow; the head has four fins so it still reads as an X when seen end-on
    void drawArrow(const WorldLines& lines, Vec3 start, Vec3 direction, Vec3 side, Vec3 up, f32 length, f32 headSize, Color color) {
        const Vec3 end = start + direction * length;
        const Vec3 headBase = end - direction * headSize;
        const f32 finSpread = headSize * 0.6f;

        lines.line(start, end, color);
        lines.line(end, headBase + side * finSpread, color);
        lines.line(end, headBase - side * finSpread, color);
        lines.line(end, headBase + up * finSpread, color);
        lines.line(end, headBase - up * finSpread, color);
    }

    // Arrows from the light along its direction: one for spot lights, three parallel ones for directional lights.
    // Sizes are in pixels and converted to world units at the light's distance, so they stay steady on screen.
    void drawDirection(const WorldLines& lines, const Light& light, const Camera& camera, f32 distance) {
        const f32 unitsPerPixel = distance * 2.0f * std::tan(camera.fovRadians * 0.5f) / lines.height;
        const Vec3 direction = light.direction.normalized();

        // Spread the side arrows across the view, falling back to any perpendicular when looking straight down the light
        Vec3 side = Vec3::cross(direction, camera.position - light.position);
        if (side.length() < 1e-4f) side = Vec3::cross(direction, std::abs(direction.y) < 0.9f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f));
        side = side.normalized();
        const Vec3 up = Vec3::cross(side, direction).normalized();

        const Vec3 start = light.position + direction * ((ORB_RADIUS + DIRECTION_GAP) * unitsPerPixel);
        const Color color = light.enabled ? lightColor(light, 0.9f) : DISABLED_COLOR;

        drawArrow(lines, start, direction, side, up, DIRECTION_LENGTH * unitsPerPixel, ARROW_SIZE * unitsPerPixel, color);

        if (light.type == LightType::Directional) {
            const Vec3 offset = side * (SIDE_ARROW_OFFSET * unitsPerPixel);
            drawArrow(lines, start + offset, direction, side, up, SIDE_ARROW_LENGTH * unitsPerPixel, SIDE_ARROW_SIZE * unitsPerPixel, color);
            drawArrow(lines, start - offset, direction, side, up, SIDE_ARROW_LENGTH * unitsPerPixel, SIDE_ARROW_SIZE * unitsPerPixel, color);
        }
    }

    void drawOrb(UIDrawList& ui, const Light& light, Vec2 orb, bool selected) {
        const f32 ringRadius = ORB_RADIUS + SELECTED_RING_GAP + SELECTED_RING_WIDTH;

        // Soft halo behind everything so the selection glows rather than just being outlined
        if (selected) {
            ui.shadow(circle(orb, ringRadius), ringRadius, SELECTED_GLOW_BLUR, SELECTED_GLOW_COLOR);
        }

        if (light.enabled) {
            ui.shadow(circle(orb, ORB_RADIUS), ORB_RADIUS, GLOW_BLUR, lightColor(light, GLOW_ALPHA));
            ui.roundedRect(circle(orb, ORB_RADIUS), ORB_RADIUS, lightColor(light, 1.0f), OUTLINE_COLOR, ORB_OUTLINE);
        } else {
            ui.roundedRect(circle(orb, ORB_RADIUS), ORB_RADIUS, { 0.0f, 0.0f, 0.0f, 0.0f }, DISABLED_COLOR, DISABLED_RING);
        }

        // Ring just outside the orb; the orb keeps its own color
        if (selected) {
            ui.roundedRect(circle(orb, ringRadius), ringRadius, { 0.0f, 0.0f, 0.0f, 0.0f }, SELECTED_COLOR, SELECTED_RING_WIDTH);
        }
    }
}

void drawLightMarkers(const AppContext& ctx, UIDrawList& ui, const Mat4& viewProjection, f32 width, f32 height) {
    const LightCollection& lights = ctx.scene.lights;
    const Vec3 cameraPosition = ctx.scene.camera.position;

    struct Marker {
        const Light* light;
        Vec2 screen;
        f32 distance;
        bool selected;
    };

    std::vector<Marker> markers;
    for (LightHandle handle : lights.handles()) {
        const Light& light = lights.get(handle);

        Vec2 screen;
        if (!projectToScreen(viewProjection, light.position, width, height, screen)) continue;

        markers.push_back({ &light, screen, (light.position - cameraPosition).length(), ctx.scene.selection.hasLight(handle) });
    }

    // Farthest first so nearer markers draw on top
    std::sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) { return a.distance > b.distance; });

    // Ground lines sit under every marker
    for (const Marker& marker : markers) {
        drawGroundLine(ui, *marker.light, marker.screen, viewProjection, width, height);
    }

    const WorldLines lines { ui, viewProjection, width, height };

    for (const Marker& marker : markers) {
        if (marker.light->type != LightType::Point) {
            drawDirection(lines, *marker.light, ctx.scene.camera, marker.distance);
        }
        drawOrb(ui, *marker.light, marker.screen, marker.selected);
    }
}
