#include "application/main_panel.hpp"
#include "application/ui_undo.hpp"
#include "application/light_commands.hpp"
#include "application/object_commands.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
    constexpr f32 PANEL_WIDTH = 330.0f;
    constexpr f32 PANEL_HEIGHT = 620.0f;
    constexpr f32 PANEL_MARGIN = 20.0f;
    constexpr f32 RADIANS_TO_DEGREES = 180.0f / 3.14159265f;

    // The object and light lists show this many rows before they scroll
    constexpr u32 LIST_ROWS = 6;
    constexpr f32 TYPE_BUTTON_WIDTH = 120.0f;
    constexpr f32 PRESET_DROPDOWN_WIDTH = 130.0f;
    constexpr f32 MIN_SCALE = 0.001f;

    const std::vector<std::string_view> TABS = { "Objects", "Lights" };
    constexpr i32 OBJECTS_TAB = 0;
    constexpr i32 LIGHTS_TAB = 1;

    f32 listHeight() {
        return LIST_ROWS * UIStyle::ROW_HEIGHT + (LIST_ROWS - 1) * UIStyle::ITEM_SPACING + UIStyle::CHILD_PADDING * 2.0f;
    }

    // A heading row with three controls on the right: picker, +, and -
    struct ListHeader {
        Rect minus;
        Rect plus;
        Rect picker;
    };

    ListHeader listHeader(UIContext& ui, std::string_view title, f32 pickerWidth) {
        const Rect header = ui.row();
        ui.text(header, title, UIStyle::ACCENT_GREEN);

        ListHeader rects;
        rects.minus = { header.right() - header.height, header.y, header.height, header.height };
        rects.plus = { rects.minus.x - UIStyle::COMPONENT_GAP - header.height, header.y, header.height, header.height };
        rects.picker = { rects.plus.x - UIStyle::COMPONENT_GAP - pickerWidth, header.y, pickerWidth, header.height };
        return rects;
    }

    std::vector<std::string_view> presetLabels() {
        std::vector<std::string_view> labels;
        for (const ObjectPreset& preset : objectPresets()) labels.push_back(preset.displayName);
        return labels;
    }

    // Heading with the preset dropdown, + and - on the right, then a fixed-height scrolling list
    void objectListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        ObjectCollection& objects = ctx.scene.objects;
        Selection& selection = ctx.scene.selection;
        i32& preset = ctx.viewport.newObjectPreset;

        const ListHeader header = listHeader(ui, "Objects", PRESET_DROPDOWN_WIDTH);
        ui.dropdown("preset", header.picker, preset, presetLabels());

        if (ui.button("+", header.plus)) {
            const ObjectPreset& chosen = objectPresets()[preset];
            addObject(ctx, chosen.preset, objects.uniqueName(chosen.displayName));
        }
        if (ui.button("-", header.minus, objects.isValid(selection.getActiveObject()))) {
            removeObject(ctx, selection.getActiveObject());
        }

        const bool objectMode = ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionObject;

        ui.beginChild("list", listHeight());
        for (ObjectHandle handle : objects.handles()) {
            const Object& object = objects.get(handle);
            const std::string detail = std::to_string(object.meshData.getFaceHandles().size()) + " faces";

            // Object mode lists the selected objects; edit modes the one being edited
            const bool selected = objectMode ? selection.hasObject(handle) : handle == selection.getActiveObject();

            ui.pushId(handle.index);
            if (ui.selectable(object.name, selected, detail)) {
                selection.clearLights();
                selection.setActiveObject(handle);
                if (objectMode) {
                    selection.clearObjects();
                    selection.selectObject(handle);
                }
            }
            ui.popId();
        }
        ui.endChild();
    }

    // Edits a copy of the transform so an undo restore mid-frame never leaves a dangling reference
    void selectedObjectSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        ObjectCollection& objects = ctx.scene.objects;
        const ObjectHandle handle = ctx.scene.selection.getActiveObject();

        ui.heading("Selected object");

        if (!objects.isValid(handle)) {
            ui.label("No object selected", true);
            return;
        }

        const Object& object = objects.get(handle);
        const MeshData& mesh = object.meshData;
        Transform transform = object.transform;
        bool changed = false;

        ui.pushId(handle.index);

        std::string name = object.name;
        const bool renamed = ui.textField("Name", name);
        trackUndo(ctx);

        ui.label(std::to_string(mesh.getVertexHandles().size()) + " verts, " + std::to_string(mesh.getEdgeHandles().size() / 2) + " edges, "
                 + std::to_string(mesh.getFaceHandles().size()) + " faces", true);

        changed |= ui.dragFloat3("Position", transform.position, 0.01f);
        trackUndo(ctx);

        Vec3 degrees = transform.rotation * RADIANS_TO_DEGREES;
        if (ui.dragFloat3("Rotation", degrees, 0.5f, "%.0f")) {
            transform.rotation = degrees / RADIANS_TO_DEGREES;
            changed = true;
        }
        trackUndo(ctx);

        // A zero scale can't be inverted for lighting, so keep each axis a little away from it
        if (ui.dragFloat3("Scale", transform.scale, 0.01f)) {
            for (f32* axis : { &transform.scale.x, &transform.scale.y, &transform.scale.z }) {
                if (std::abs(*axis) < MIN_SCALE) *axis = *axis < 0.0f ? -MIN_SCALE : MIN_SCALE;
            }
            changed = true;
        }
        trackUndo(ctx);

        ui.popId();

        if (changed && objects.isValid(handle)) objects.get(handle).transform = transform;
        if (renamed && objects.isValid(handle)) objects.get(handle).name = name;
    }

    // Order used by the type buttons and the + toggle
    const std::vector<LightType> LIGHT_TYPES = { LightType::Point, LightType::Spot, LightType::Directional };
    const std::vector<std::string_view> LIGHT_TYPE_LABELS = { "Point", "Spot", "Directional" };

    i32 typeIndex(LightType type) {
        return static_cast<i32>(std::find(LIGHT_TYPES.begin(), LIGHT_TYPES.end(), type) - LIGHT_TYPES.begin());
    }

    const char* typeName(LightType type) {
        switch (type) {
            case LightType::Point: return "point";
            case LightType::Directional: return "directional";
            case LightType::Spot: return "spot";
        }
        return "";
    }

    void addLight(AppContext& ctx, LightType type) {
        LightCollection& lights = ctx.scene.lights;

        // Spread new lights out a little so they don't all land on the same spot
        Light light;
        light.type = type;
        light.name = lights.nextName();
        light.position.x = (static_cast<f32>(lights.count() % 5) - 2.0f) * 0.8f;

        ctx.history.begin(ctx.scene);
        const LightHandle handle = lights.add(light);
        ctx.history.commit();

        ctx.scene.selection.clear();
        ctx.scene.selection.addLight(handle);
    }

    // Heading with the type toggle and + button on the right, then a fixed-height scrolling list
    void lightListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;
        Selection& selection = ctx.scene.selection;
        LightType& newType = ctx.viewport.newLightType;

        const ListHeader header = listHeader(ui, "Lights", TYPE_BUTTON_WIDTH);

        if (ui.button(LIGHT_TYPE_LABELS[typeIndex(newType)], header.picker)) {
            newType = LIGHT_TYPES[(typeIndex(newType) + 1) % LIGHT_TYPES.size()];
        }
        if (ui.button("+", header.plus)) addLight(ctx, newType);
        if (ui.button("-", header.minus, selection.hasLights())) deleteSelectedLights(ctx);

        ui.beginChild("list", listHeight());
        for (LightHandle handle : lights.handles()) {
            const Light& light = lights.get(handle);

            ui.pushId(handle.index);
            if (ui.selectable(light.name, selection.hasLight(handle), typeName(light.type))) {
                selection.clear();
                selection.addLight(handle);
            }
            ui.popId();
        }
        ui.endChild();
    }

    // Edits a copy so an undo restore mid-frame never leaves a dangling reference
    void selectedLightSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;
        const std::vector<LightHandle>& selected = ctx.scene.selection.getLights();

        ui.heading("Selected light");

        if (selected.empty() || !lights.isValid(selected.front())) {
            ui.label("No light selected", true);
            return;
        }

        const LightHandle handle = selected.front();
        Light light = lights.get(handle);
        bool changed = false;

        ui.pushId(handle.index);

        changed |= ui.textField("Name", light.name);
        trackUndo(ctx);

        i32 type = typeIndex(light.type);
        if (ui.segmented("", type, LIGHT_TYPE_LABELS)) {
            light.type = LIGHT_TYPES[type];
            changed = true;
        }
        trackUndo(ctx);

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
}

void drawMainPanel(AppContext& ctx, const Rect& bounds) {
    UIContext& ui = ctx.ui;
    UIPanelState& panel = ctx.viewport.panel;

    // First use: top right of the viewport
    if (panel.rect.width == 0.0f) {
        const f32 height = std::min(PANEL_HEIGHT, bounds.height - PANEL_MARGIN * 2.0f);
        panel.rect = { bounds.right() - PANEL_WIDTH - PANEL_MARGIN, bounds.y + PANEL_MARGIN, PANEL_WIDTH, height };
    }

    ui.setViewport(bounds);
    ui.beginPanel("main panel", panel, bounds, TABS);

    // Each section gets its own ID scope so labels like "Strength" can repeat across sections
    if (panel.activeTab == OBJECTS_TAB) {
        ui.pushId("objects");
        objectListSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("selected object");
        selectedObjectSection(ctx);
        ui.popId();
    } else if (panel.activeTab == LIGHTS_TAB) {
        ui.pushId("lights");
        lightListSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("selected light");
        selectedLightSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("ambient");
        ambientSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("headlight");
        headlightSection(ctx);
        ui.popId();
    }

    ui.endPanel();
}
