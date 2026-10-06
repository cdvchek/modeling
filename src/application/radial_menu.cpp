#include "application/radial_menu.hpp"
#include "core/math/math_utils.hpp"
#include "ui/ui_style.hpp"

#include <string>

namespace {
    constexpr f32 DEAD_ZONE = 30.0f;
    constexpr f32 INNER_RADIUS = 40.0f;
    constexpr f32 OUTER_RADIUS = 92.0f;
    constexpr f32 SLICE_GAP = 6.0f;
    // Moving this far toward a submenu opens it, re-centered at the cursor
    constexpr f32 SUBMENU_RADIUS = 104.0f;
    constexpr f32 LABEL_RADIUS = 106.0f;
    constexpr f32 LABEL_PADDING_X = 10.0f;
    constexpr f32 LABEL_PADDING_Y = 6.0f;
    constexpr f32 CENTER_DOT = 5.0f;

    const Color SLICE_FILL { 0.14f, 0.14f, 0.16f, 0.92f };
    const Color SLICE_DIM_FILL { 0.14f, 0.14f, 0.16f, 0.6f };
    const Color TEXT_DISABLED { 0.55f, 0.55f, 0.60f, 0.5f };
    const Color HINT { 0.55f, 0.55f, 0.60f, 0.7f };

    RadialItem item(const ActionMap& actions, Action action) {
        const ActionHandler* handler = actions.getHandler(action);
        return { handler ? handler->label : "?", action, RadialMenuId::None };
    }

    // An action shown with a label of its own, e.g. one that says what a toggle will do
    RadialItem toggle(Action action, std::string_view label) {
        return { label, action, RadialMenuId::None };
    }

    RadialItem submenu(std::string_view label, RadialMenuId id) {
        return { label, Action::Count, id };
    }

    bool isAvailable(const AppContext& ctx, const RadialItem& item) {
        if (item.submenu != RadialMenuId::None) return true;
        if (item.action == Action::Count) return false;
        return ctx.systems.actions.isAvailable(item.action, ctx.systems.input_ctx.getContext());
    }

    RadialMenuId rootMenu(const AppContext& ctx) {
        const bool toolRunning = ctx.systems.input_ctx.isActive(InputContext_Grab | InputContext_Scale | InputContext_Rotate | InputContext_Bevel | InputContext_Inset);
        return toolRunning ? RadialMenuId::Tool : RadialMenuId::Main;
    }

    Vec2 mousePosition(const AppContext& ctx) {
        return Vec2(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    }

    void close(RadialMenuState& menu) {
        menu.open = false;
        menu.hovered = -1;
    }
}

// Each menu lists its items clockwise from the top
RadialMenu buildRadialMenu(const AppContext& ctx, RadialMenuId id) {
    const ActionMap& actions = ctx.systems.actions;

    switch (id) {
        case RadialMenuId::Main:
            return {
                item(actions, Action::GrabSelection),
                item(actions, Action::ScaleSelection),
                ctx.scene.selection.hasLights() ? submenu("Light", RadialMenuId::Light) : submenu("Edit", RadialMenuId::Edit),
                submenu("View", RadialMenuId::View),
                submenu("Mode", RadialMenuId::Mode),
                item(actions, Action::Redo),
                item(actions, Action::Undo),
                item(actions, Action::RotateSelection),
            };

        case RadialMenuId::Edit:
            return {
                item(actions, Action::ExtrudeSelection),
                item(actions, Action::InsetSelection),
                item(actions, Action::FillFaceLoop),
                item(actions, Action::DissolveSelection),
                item(actions, Action::DeleteSelection),
                item(actions, Action::MergeVertices),
                item(actions, Action::ConnectVertices),
                item(actions, Action::BevelSelection),
            };

        case RadialMenuId::Light:
            return {
                item(actions, Action::LightSpot),
                item(actions, Action::LightDirectional),
                item(actions, Action::ToggleLights),
                item(actions, Action::DeleteSelection),
                item(actions, Action::LightPoint),
            };

        case RadialMenuId::Mode:
            return {
                item(actions, Action::EdgeMode),
                item(actions, Action::FaceMode),
                item(actions, Action::ObjectMode),
                item(actions, Action::VertexMode),
            };

        case RadialMenuId::View:
            return {
                toggle(Action::TogglePanel, ctx.viewport.showPanel ? "Hide panel" : "Show panel"),
                toggle(Action::ToggleHeadlight, ctx.viewport.headlight.enabled ? "Headlight off" : "Headlight on"),
                toggle(Action::ToggleDebug, ctx.systems.input_ctx.isActive(InputContext_Debug) ? "Debug off" : "Debug on"),
            };

        case RadialMenuId::Tool:
            return {
                item(actions, Action::YAxis),
                item(actions, Action::ZAxis),
                item(actions, Action::AxisFree),
                item(actions, Action::XAxis),
            };

        case RadialMenuId::None:
            break;
    }

    return {};
}

bool updateRadialMenu(AppContext& ctx) {
    RadialMenuState& menu = ctx.radialMenu;
    const ActionMap& actions = ctx.systems.actions;
    const InputState& input = ctx.systems.input;
    const u32 contexts = ctx.systems.input_ctx.getContext();

    if (!menu.open) {
        if (!actions.wasActionPressedThisFrame(Action::RadialMenu, input, contexts)) return false;

        menu.open = true;
        menu.menu = rootMenu(ctx);
        menu.center = mousePosition(ctx);
        menu.hovered = -1;
        return true;
    }

    // Right click closes without running anything
    if (input.wasMousePressedThisFrame(static_cast<u16>(MouseButton::Right))) {
        close(menu);
        return true;
    }

    const RadialMenu items = buildRadialMenu(ctx, menu.menu);
    const Vec2 offset = mousePosition(ctx) - menu.center;

    menu.hovered = RadialLayout::sliceAt(offset, DEAD_ZONE, static_cast<u32>(items.size()));

    if (menu.hovered >= 0) {
        const RadialItem& hovered = items[menu.hovered];
        if (hovered.submenu != RadialMenuId::None && offset.length() >= SUBMENU_RADIUS) {
            menu.menu = hovered.submenu;
            menu.center = mousePosition(ctx);
            menu.hovered = -1;
            return true;
        }
    }

    if (!actions.isActionDown(Action::RadialMenu, input, contexts)) {
        if (menu.hovered >= 0) {
            const RadialItem& picked = items[menu.hovered];
            if (picked.submenu == RadialMenuId::None && isAvailable(ctx, picked)) actions.getHandler(picked.action)->run();
        }
        close(menu);
    }

    return true;
}

void drawRadialMenu(const AppContext& ctx, UIDrawList& ui) {
    const RadialMenuState& menu = ctx.radialMenu;
    if (!menu.open) return;

    const UIFont font = makeUIFont(FontId::UI, ctx.fonts.get(FontId::UI));
    const RadialMenu items = buildRadialMenu(ctx, menu.menu);
    const u32 count = static_cast<u32>(items.size());
    if (count == 0) return;
    const f32 halfAngle = Math::PI / static_cast<f32>(count);

    ui.shadow({ menu.center.x - OUTER_RADIUS, menu.center.y - OUTER_RADIUS, OUTER_RADIUS * 2.0f, OUTER_RADIUS * 2.0f }, OUTER_RADIUS, UIStyle::PANEL_SHADOW_BLUR, UIStyle::PANEL_SHADOW);

    for (u32 slice = 0; slice < count; ++slice) {
        const RadialItem& entry = items[slice];
        const f32 angle = RadialLayout::sliceAngle(slice, count);

        const bool available = isAvailable(ctx, entry);
        const bool highlighted = available && menu.hovered == static_cast<i32>(slice);

        if (highlighted) {
            ui.ringSlice(menu.center, INNER_RADIUS, OUTER_RADIUS, angle, halfAngle, SLICE_GAP, UIStyle::ACCENT_FILL, UIStyle::ACCENT, 1.0f);
        } else {
            ui.ringSlice(menu.center, INNER_RADIUS, OUTER_RADIUS, angle, halfAngle, SLICE_GAP, available ? SLICE_FILL : SLICE_DIM_FILL, UIStyle::PANEL_BORDER, 1.0f);
        }

        // Label, then a faint key hint; submenus get a ">" instead
        std::string label(entry.label);
        if (entry.submenu != RadialMenuId::None) label += " >";

        std::string hint;
        if (entry.action != Action::Count) {
            if (const Keybind* keybind = ctx.systems.actions.getKeybind(entry.action)) hint = keybindLabel(*keybind);
        }
        if (hint == label) hint.clear();

        const f32 labelWidth = static_cast<f32>(label.size()) * font.glyphWidth;
        const f32 hintWidth = hint.empty() ? 0.0f : static_cast<f32>(hint.size() + 1) * font.glyphWidth;
        const Vec2 size(labelWidth + hintWidth + LABEL_PADDING_X * 2.0f, font.glyphHeight + LABEL_PADDING_Y * 2.0f);

        // Anchored on the slice's direction so labels grow away from the ring
        const Vec2 direction = RadialLayout::sliceDirection(slice, count);
        const Vec2 anchor = menu.center + direction * LABEL_RADIUS;
        const Rect box {
            anchor.x - size.x * 0.5f + direction.x * size.x * 0.5f,
            anchor.y - size.y * 0.5f + direction.y * size.y * 0.5f,
            size.x, size.y
        };

        ui.roundedRect(box, UIStyle::CORNER_RADIUS, UIStyle::PANEL_BACKGROUND, highlighted ? UIStyle::ACCENT : UIStyle::PANEL_BORDER, 1.0f);

        const Color labelColor = !available ? TEXT_DISABLED : highlighted ? UIStyle::ACCENT : UIStyle::TEXT;
        ui.text(Vec2(box.x + LABEL_PADDING_X, box.y + LABEL_PADDING_Y), label, font, labelColor);
        if (!hint.empty()) ui.text(Vec2(box.x + LABEL_PADDING_X + labelWidth + font.glyphWidth, box.y + LABEL_PADDING_Y), hint, font, available ? HINT : TEXT_DISABLED);
    }

    // Releasing on the center dot cancels
    ui.roundedRect({ menu.center.x - CENTER_DOT, menu.center.y - CENTER_DOT, CENTER_DOT * 2.0f, CENTER_DOT * 2.0f }, CENTER_DOT, UIStyle::TEXT_DIM);
}
