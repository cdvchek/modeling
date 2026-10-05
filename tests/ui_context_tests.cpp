#include "test.hpp"
#include "ui/ui_context.hpp"
#include "ui/ui_style.hpp"

#include <functional>

namespace {
    // A 300x400 region at the origin; with 10 px padding the first row spans x 10..290, y 10..34
    const Rect REGION { 0.0f, 0.0f, 300.0f, 400.0f };

    struct Harness {
        UIContext ui;
        UIDrawList list;
        Vec2 lastMouse;

        Harness() {
            ui.setDrawList(&list);
            ui.setFont({ FontId::UI, 10.0f, 16.0f });
        }

        void frame(Vec2 mouse, bool down, bool wasDown, const std::function<void()>& widgets, bool interactive = true) {
            UIInput input;
            input.mouse = mouse;
            input.mouseDelta = mouse - lastMouse;
            input.down[UIInput::LEFT] = down;
            input.pressed[UIInput::LEFT] = down && !wasDown;
            input.released[UIInput::LEFT] = !down && wasDown;
            lastMouse = mouse;

            ui.beginFrame(input, interactive);
            list.clear();
            ui.beginDraw();
            ui.beginRegion(REGION);
            widgets();
            ui.endRegion();
            ui.endDraw();
        }
    };

    void none() {}
}

TEST_CASE(ui_button_clicks_on_release) {
    Harness h;
    bool clicked = false;
    bool activated = false;
    const auto widgets = [&]() { clicked = h.ui.button("Add"); activated = h.ui.isItemActivated(); };

    h.frame(Vec2(50, 20), false, false, widgets);
    CHECK(!clicked);

    h.frame(Vec2(50, 20), true, false, widgets);
    CHECK(!clicked);
    CHECK(activated);
    CHECK(h.ui.wantsMouse());

    h.frame(Vec2(50, 20), false, true, widgets);
    CHECK(clicked);
}

TEST_CASE(ui_button_release_outside_does_not_click) {
    Harness h;
    bool clicked = false;
    const auto widgets = [&]() { clicked = h.ui.button("Add"); };

    h.frame(Vec2(50, 20), false, false, widgets);
    h.frame(Vec2(50, 20), true, false, widgets);
    h.frame(Vec2(50, 200), false, true, widgets);
    CHECK(!clicked);
}

TEST_CASE(ui_wants_mouse_only_over_regions) {
    Harness h;
    h.frame(Vec2(50, 20), false, false, none);
    h.frame(Vec2(50, 20), false, false, none);
    CHECK(h.ui.wantsMouse());

    h.frame(Vec2(500, 20), false, false, none);
    CHECK(!h.ui.wantsMouse());
}

TEST_CASE(ui_viewport_drag_passing_over_ui_stays_with_viewport) {
    Harness h;
    bool hovered = false;
    const auto widgets = [&]() { h.ui.button("Add"); hovered = h.ui.isItemHovered(); };

    h.frame(Vec2(500, 20), false, false, widgets);
    h.frame(Vec2(500, 20), true, false, widgets);
    CHECK(!h.ui.wantsMouse());

    h.frame(Vec2(50, 20), true, true, widgets);
    CHECK(!h.ui.wantsMouse());
    CHECK(!hovered);

    h.frame(Vec2(50, 20), false, true, widgets);
    h.frame(Vec2(50, 20), false, false, widgets);
    CHECK(h.ui.wantsMouse());
    CHECK(hovered);
}

TEST_CASE(ui_slider_holds_on_press_and_tracks_outside) {
    Harness h;
    f32 value = 0.5f;
    bool activated = false;
    bool afterEdit = false;
    const auto widgets = [&]() {
        h.ui.sliderFloat("Intensity", value, 0.0f, 1.0f);
        activated = h.ui.isItemActivated();
        afterEdit = h.ui.isItemDeactivatedAfterEdit();
    };

    h.frame(Vec2(200, 20), false, false, widgets);

    // Press: activates but doesn't change the value yet
    h.frame(Vec2(150, 20), true, false, widgets);
    CHECK(activated);
    CHECK(value == 0.5f);

    // Dragging past the right edge, outside the region, still drives the slider
    h.frame(Vec2(600, 300), true, true, widgets);
    CHECK(value == 1.0f);
    CHECK(h.ui.wantsMouse());

    // The release frame still belongs to the slider; after that the viewport gets the mouse back
    h.frame(Vec2(600, 300), false, true, widgets);
    CHECK(afterEdit);
    h.frame(Vec2(600, 300), false, false, widgets);
    CHECK(!h.ui.wantsMouse());
}

TEST_CASE(ui_checkbox_toggles_on_release) {
    Harness h;
    bool value = false;
    bool afterEdit = false;
    const auto widgets = [&]() { h.ui.checkbox("Enabled", value); afterEdit = h.ui.isItemDeactivatedAfterEdit(); };

    h.frame(Vec2(150, 20), false, false, widgets);
    h.frame(Vec2(150, 20), true, false, widgets);
    CHECK(!value);

    h.frame(Vec2(150, 20), false, true, widgets);
    CHECK(value);
    CHECK(afterEdit);
}

TEST_CASE(ui_drag_float3_moves_by_mouse_delta) {
    Harness h;
    Vec3 value(1.0f, 2.0f, 3.0f);
    const auto widgets = [&]() { h.ui.dragFloat3("Position", value, 0.01f); };

    // Control column starts at x = 10 + floor(280 * 0.38) = 116; the X box is first
    h.frame(Vec2(130, 20), false, false, widgets);
    h.frame(Vec2(130, 20), true, false, widgets);
    h.frame(Vec2(230, 20), true, true, widgets);
    h.frame(Vec2(230, 20), false, true, widgets);

    CHECK(std::abs(value.x - 2.0f) < 1e-4f);
    CHECK(value.y == 2.0f);
    CHECK(value.z == 3.0f);
}

TEST_CASE(ui_ids_differ_by_scope) {
    UIContext ui;
    ui.pushId(1u);
    const UIId first = ui.makeId("Intensity");
    ui.popId();
    ui.pushId(2u);
    const UIId second = ui.makeId("Intensity");
    ui.popId();

    CHECK(first != second);
    CHECK(first != 0);
}

TEST_CASE(ui_not_interactive_ignores_mouse) {
    Harness h;
    bool activated = false;
    const auto widgets = [&]() { h.ui.button("Add"); activated = h.ui.isItemActivated(); };

    h.frame(Vec2(50, 20), false, false, widgets, false);
    h.frame(Vec2(50, 20), true, false, widgets, false);
    CHECK(!activated);
    CHECK(!h.ui.wantsMouse());
}

TEST_CASE(ui_active_widget_that_disappears_lets_go) {
    Harness h;
    bool show = true;
    const auto widgets = [&]() { if (show) h.ui.button("Delete"); };

    h.frame(Vec2(50, 20), false, false, widgets);
    h.frame(Vec2(50, 20), true, false, widgets);
    CHECK(h.ui.getActiveId() != 0);

    show = false;
    h.frame(Vec2(50, 20), true, true, widgets);
    CHECK(h.ui.getActiveId() == 0);
}

TEST_CASE(ui_panel_drags_by_header_and_stays_inside) {
    UIContext ui;
    UIDrawList list;
    ui.setDrawList(&list);
    ui.setFont({ FontId::UI, 10.0f, 16.0f });

    UIPanelState state { { 100.0f, 100.0f, 200.0f, 300.0f } };
    const Rect bounds { 0.0f, 0.0f, 800.0f, 600.0f };
    Vec2 lastMouse;

    const auto frame = [&](Vec2 mouse, bool down, bool wasDown) {
        UIInput input;
        input.mouse = mouse;
        input.mouseDelta = mouse - lastMouse;
        input.down[UIInput::LEFT] = down;
        input.pressed[UIInput::LEFT] = down && !wasDown;
        input.released[UIInput::LEFT] = !down && wasDown;
        lastMouse = mouse;

        ui.beginFrame(input, true);
        list.clear();
        ui.beginDraw();
        ui.beginPanel("panel", state, bounds, "Title");
        ui.endPanel();
        ui.endDraw();
    };

    // Grab the header 10 px in, then move: the panel keeps that offset
    frame(Vec2(110, 110), false, false);
    frame(Vec2(110, 110), true, false);
    frame(Vec2(160, 130), true, true);
    CHECK(state.rect.x == 150.0f);
    CHECK(state.rect.y == 120.0f);

    // Dragged far past the corner, it stops at the bounds
    frame(Vec2(5000, 5000), true, true);
    CHECK(state.rect.x == 600.0f);
    CHECK(state.rect.y == 300.0f);
    frame(Vec2(5000, 5000), false, true);

    // Dragging the body (below the header) doesn't move it
    frame(Vec2(700, 500), false, false);
    frame(Vec2(700, 500), true, false);
    frame(Vec2(650, 450), true, true);
    CHECK(state.rect.x == 600.0f);
    CHECK(state.rect.y == 300.0f);
    frame(Vec2(650, 450), false, true);

    // A smaller window pulls it back inside on the next frame
    const Rect smaller { 0.0f, 0.0f, 500.0f, 400.0f };
    ui.beginFrame(UIInput {}, true);
    ui.beginDraw();
    ui.beginPanel("panel", state, smaller, "Title");
    ui.endPanel();
    ui.endDraw();
    CHECK(state.rect.x == 300.0f);
    CHECK(state.rect.y == 100.0f);
}

namespace {
    struct PanelHarness {
        UIContext ui;
        UIDrawList list;
        UIPanelState state { { 100.0f, 100.0f, 300.0f, 300.0f } };
        Rect bounds { 0.0f, 0.0f, 800.0f, 600.0f };
        Vec2 lastMouse;

        PanelHarness() {
            ui.setDrawList(&list);
            ui.setFont({ FontId::UI, 10.0f, 16.0f });
        }

        void frame(Vec2 mouse, bool down, bool wasDown) {
            UIInput input;
            input.mouse = mouse;
            input.mouseDelta = mouse - lastMouse;
            input.down[UIInput::LEFT] = down;
            input.pressed[UIInput::LEFT] = down && !wasDown;
            input.released[UIInput::LEFT] = !down && wasDown;
            lastMouse = mouse;

            ui.beginFrame(input, true);
            list.clear();
            ui.beginDraw();
            ui.beginPanel("panel", state, bounds, "Title");
            ui.endPanel();
            ui.endDraw();
        }

        // Hover, press, move, release
        void drag(Vec2 from, Vec2 to) {
            frame(from, false, false);
            frame(from, true, false);
            frame(to, true, true);
            frame(to, false, true);
        }
    };
}

TEST_CASE(ui_panel_resizes_from_right_edge) {
    PanelHarness h;
    h.frame(Vec2(398, 250), false, false);
    h.frame(Vec2(398, 250), false, false);
    CHECK(h.ui.cursor() == UICursor::ResizeHorizontal);

    h.drag(Vec2(398, 250), Vec2(498, 250));
    CHECK(h.state.rect.x == 100.0f);
    CHECK(h.state.rect.width == 400.0f);
    CHECK(h.state.rect.height == 300.0f);
}

TEST_CASE(ui_panel_left_edge_stops_at_min_width) {
    PanelHarness h;
    h.drag(Vec2(101, 250), Vec2(390, 250));
    CHECK(h.state.rect.width == UIStyle::PANEL_MIN_WIDTH);
    CHECK(h.state.rect.right() == 400.0f);
}

TEST_CASE(ui_panel_corner_resizes_both_ways_within_bounds) {
    PanelHarness h;
    h.frame(Vec2(399, 399), false, false);
    h.frame(Vec2(399, 399), false, false);
    CHECK(h.ui.cursor() == UICursor::ResizeDiagonalDown);

    h.drag(Vec2(399, 399), Vec2(2000, 2000));
    CHECK(h.state.rect.right() == 800.0f);
    CHECK(h.state.rect.bottom() == 600.0f);
    CHECK(h.state.rect.x == 100.0f);
    CHECK(h.state.rect.y == 100.0f);
}

TEST_CASE(ui_panel_top_edge_resizes_instead_of_moving) {
    PanelHarness h;
    h.drag(Vec2(250, 101), Vec2(250, 51));
    CHECK(h.state.rect.y == 50.0f);
    CHECK(h.state.rect.height == 350.0f);
    CHECK(h.state.rect.bottom() == 400.0f);
}

TEST_CASE(ui_panel_shrinks_to_fit_smaller_bounds) {
    PanelHarness h;
    h.bounds = { 0.0f, 0.0f, 250.0f, 200.0f };
    h.frame(Vec2(700, 500), false, false);
    CHECK(h.state.rect.width == 250.0f);
    CHECK(h.state.rect.height == 200.0f);
    CHECK(h.state.rect.x == 0.0f);
    CHECK(h.state.rect.y == 0.0f);
}
