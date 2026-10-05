#include "application/main_panel.hpp"
#include "application/ui_undo.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <string>

namespace {
    constexpr f32 PANEL_WIDTH = 330.0f;
    constexpr f32 PANEL_HEIGHT = 620.0f;
    constexpr f32 PANEL_MARGIN = 20.0f;
    constexpr f32 RADIANS_TO_DEGREES = 180.0f / 3.14159265f;

    const char* typeName(LightType type) {
        switch (type) {
            case LightType::Point: return "point";
            case LightType::Directional: return "directional";
            case LightType::Spot: return "spot";
        }
        return "";
    }

    void ambientSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;

        ui.heading("Ambient");

        f32 strength = lights.getAmbient().strength;
        if (ui.sliderFloat("Strength", strength, 0.0f, 1.0f)) lights.setAmbientStrength(strength);
        trackUndo(ctx);

        Vec3 color = lights.getAmbient().color;
        if (ui.colorEdit("Color", color)) lights.setAmbientColor(color);
        trackUndo(ctx);
    }

    // View-only setting, so no undo
    void headlightSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        Headlight& headlight = ctx.viewport.headlight;

        ui.heading("Headlight");
        ui.checkbox("Enabled", headlight.enabled);
        ui.sliderFloat("Strength", headlight.strength, 0.0f, 1.0f);
        ui.colorEdit("Color", headlight.color);
    }

    void lightListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;
        Selection& selection = ctx.scene.selection;

        ui.heading("Lights");

        for (LightHandle handle : lights.handles()) {
            const Light& light = lights.get(handle);
            const std::string text = light.name + "  (" + typeName(light.type) + ")";

            ui.pushId(handle.index);
            if (ui.selectable(text, selection.hasLight(handle))) {
                selection.clear();
                selection.addLight(handle);
            }
            ui.popId();
        }

        if (ui.button("Add point light")) {
            Light light;
            light.name = "point";
            light.position = Vec3(0.0f, 1.5f, 1.0f);

            ctx.history.begin(ctx.scene);
            const LightHandle handle = lights.add(light);
            ctx.history.commit();

            selection.clear();
            selection.addLight(handle);
        }
    }

    // Edits a copy so an undo restore mid-frame never leaves a dangling reference
    void selectedLightSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;
        const std::vector<LightHandle>& selected = ctx.scene.selection.getLights();
        if (selected.empty() || !lights.isValid(selected.front())) return;

        const LightHandle handle = selected.front();
        Light light = lights.get(handle);
        bool changed = false;

        ui.heading("Selected light");
        ui.pushId(handle.index);

        changed |= ui.checkbox("Enabled", light.enabled);
        trackUndo(ctx);
        changed |= ui.colorEdit("Color", light.color);
        trackUndo(ctx);
        changed |= ui.sliderFloat("Intensity", light.intensity, 0.0f, 5.0f);
        trackUndo(ctx);

        if (light.type != LightType::Directional) {
            changed |= ui.dragFloat3("Position", light.position, 0.01f);
            trackUndo(ctx);
            changed |= ui.sliderFloat("Range", light.range, 0.1f, 30.0f, "%.1f");
            trackUndo(ctx);
        }

        if (light.type != LightType::Point) {
            if (ui.dragFloat3("Direction", light.direction, 0.01f)) {
                if (light.direction.length() > 1e-4f) light.direction = light.direction.normalized();
                changed = true;
            }
            trackUndo(ctx);
        }

        if (light.type == LightType::Spot) {
            f32 inner = light.innerConeRadians * RADIANS_TO_DEGREES;
            f32 outer = light.outerConeRadians * RADIANS_TO_DEGREES;

            if (ui.sliderFloat("Inner cone", inner, 0.0f, 89.0f, "%.0f")) {
                light.innerConeRadians = std::min(inner, outer) / RADIANS_TO_DEGREES;
                changed = true;
            }
            trackUndo(ctx);

            if (ui.sliderFloat("Outer cone", outer, 1.0f, 89.0f, "%.0f")) {
                light.outerConeRadians = outer / RADIANS_TO_DEGREES;
                light.innerConeRadians = std::min(light.innerConeRadians, light.outerConeRadians);
                changed = true;
            }
            trackUndo(ctx);
        }

        ui.popId();

        if (changed && lights.isValid(handle)) lights.replace(handle, light);
    }
}

void drawMainPanel(AppContext& ctx, const Rect& bounds) {
    UIContext& ui = ctx.ui;
    UIPanelState& panel = ctx.viewport.panel;

    // First use: top right of the viewport
    if (panel.rect.width == 0.0f) {
        const f32 height = std::min(PANEL_HEIGHT, bounds.height - PANEL_MARGIN * 2.0f);
        panel.rect = { bounds.right() - PANEL_WIDTH - PANEL_MARGIN, bounds.y + PANEL_MARGIN, PANEL_WIDTH, height };
    }

    ui.beginPanel("main panel", panel, bounds, "Lights");

    ambientSection(ctx);
    ui.spacing();
    headlightSection(ctx);
    ui.spacing();
    lightListSection(ctx);
    ui.spacing();
    selectedLightSection(ctx);

    ui.endPanel();
}
