#include "test.hpp"
#include "ui/ui_context.hpp"
#include "ui/ui_style.hpp"

#include <cmath>
#include <functional>

namespace {
    // A 300x400 region at the origin; with 10 px padding the first row spans x 10..290, y 10..34
    const Rect REGION { 0.0f, 0.0f, 300.0f, 400.0f };

    struct Harness {
        UIContext ui;
        UIDrawList list;
        Vec2 lastMouse;

        // Keyboard input for the next frame only
        std::string typed;
        std::vector<UIKey> keys;
        bool shift = false;

        Harness() {
            ui.setDrawList(&list);
            ui.setFont({ FontId::UI, 10.0f, 16.0f });
        }

        void frame(Vec2 mouse, bool down, bool wasDown, const std::function<void()>& widgets, bool interactive = true) {
            UIInput input;
            input.text = typed;
            input.keys = keys;
            input.shift = shift;
            typed.clear();
            keys.clear();
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

TEST_CASE(ui_drag_float_is_one_box_across_the_control) {
    Harness h;
    f32 value = 2.0f;
    const auto widgets = [&]() { h.ui.dragFloat("Size", value, 0.01f); };

    // One box fills the control column, so a press near its right end still drags it
    h.frame(Vec2(270, 20), false, false, widgets);
    h.frame(Vec2(270, 20), true, false, widgets);
    h.frame(Vec2(220, 20), true, true, widgets);
    h.frame(Vec2(220, 20), false, true, widgets);

    CHECK(std::abs(value - 1.5f) < 1e-4f);
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

        std::function<void()> content = []() {};

        PanelHarness() {
            ui.setDrawList(&list);
            ui.setFont({ FontId::UI, 10.0f, 16.0f });
        }

        void frame(Vec2 mouse, bool down, bool wasDown, i32 scroll = 0) {
            UIInput input;
            input.mouse = mouse;
            input.scroll = scroll;
            input.mouseDelta = mouse - lastMouse;
            input.down[UIInput::LEFT] = down;
            input.pressed[UIInput::LEFT] = down && !wasDown;
            input.released[UIInput::LEFT] = !down && wasDown;
            lastMouse = mouse;

            ui.beginFrame(input, true);
            list.clear();
            ui.beginDraw();
            ui.beginPanel("panel", state, bounds, "Title");
            content();
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

TEST_CASE(ui_panel_scrolls_with_wheel_within_limits) {
    PanelHarness h;
    h.content = [&]() { for (u32 i = 0; i < 20; ++i) { h.ui.pushId(i); h.ui.button("Row"); h.ui.popId(); } };

    // 20 rows of 24 px with 4 px gaps = 556 px of content; the 300 px panel shows 248 px under its header
    h.frame(Vec2(250, 250), false, false);
    h.frame(Vec2(250, 250), false, false);
    CHECK(h.state.contentHeight == 556.0f);

    h.frame(Vec2(250, 250), false, false, -120);
    CHECK(h.state.scroll == UIStyle::SCROLL_STEP);

    // Lots of scrolling stops at the bottom, then scrolling up stops at the top
    for (u32 i = 0; i < 20; ++i) h.frame(Vec2(250, 250), false, false, -120);
    CHECK(h.state.scroll == 556.0f - 248.0f);
    for (u32 i = 0; i < 20; ++i) h.frame(Vec2(250, 250), false, false, 120);
    CHECK(h.state.scroll == 0.0f);

    // The wheel outside the panel does nothing
    h.frame(Vec2(700, 500), false, false, -120);
    h.frame(Vec2(700, 500), false, false, -120);
    CHECK(h.state.scroll == 0.0f);
}

TEST_CASE(ui_panel_hidden_rows_cannot_be_clicked_through_header) {
    PanelHarness h;
    bool firstClicked = false;
    h.content = [&]() {
        for (u32 i = 0; i < 20; ++i) {
            h.ui.pushId(i);
            const bool clicked = h.ui.button("Row");
            if (i == 0 && clicked) firstClicked = true;
            h.ui.popId();
        }
    };

    // Scroll so row 0 sits under the header, then click the header there
    h.frame(Vec2(250, 250), false, false);
    h.frame(Vec2(250, 250), false, false, -120);
    h.frame(Vec2(250, 250), false, false);
    h.drag(Vec2(250, 115), Vec2(250, 115));
    CHECK(!firstClicked);
}

TEST_CASE(ui_panel_scrollbar_thumb_drags) {
    PanelHarness h;
    h.content = [&]() { for (u32 i = 0; i < 20; ++i) { h.ui.pushId(i); h.ui.button("Row"); h.ui.popId(); } };

    h.frame(Vec2(250, 250), false, false);
    h.frame(Vec2(250, 250), false, false);

    // Thumb sits at the top of the track (x = 400 - 5 - 4 = 391); drag it all the way down
    h.drag(Vec2(393, 150), Vec2(393, 2000));
    CHECK(h.state.scroll == 556.0f - 248.0f);
}

TEST_CASE(ui_same_label_in_different_scopes_edits_independently) {
    Harness h;
    f32 ambient = 0.5f;
    f32 headlight = 0.5f;
    const auto widgets = [&]() {
        h.ui.pushId("ambient");
        h.ui.sliderFloat("Strength", ambient, 0.0f, 1.0f);
        h.ui.popId();
        h.ui.pushId("headlight");
        h.ui.sliderFloat("Strength", headlight, 0.0f, 1.0f);
        h.ui.popId();
    };

    // Drag the first slider (row 1, y 10..34) all the way right
    h.frame(Vec2(150, 20), false, false, widgets);
    h.frame(Vec2(150, 20), true, false, widgets);
    h.frame(Vec2(600, 20), true, true, widgets);
    h.frame(Vec2(600, 20), false, true, widgets);

    CHECK(ambient == 1.0f);
    CHECK(headlight == 0.5f);
}

TEST_CASE(ui_child_list_takes_the_wheel_before_the_panel) {
    PanelHarness h;
    h.content = [&]() {
        h.ui.beginChild("list", 100.0f);
        for (u32 i = 0; i < 10; ++i) { h.ui.pushId(i); h.ui.selectable("Row", false); h.ui.popId(); }
        h.ui.endChild();
        for (u32 i = 0; i < 20; ++i) { h.ui.pushId(i); h.ui.button("Below"); h.ui.popId(); }
    };

    // The list box spans y 142..242 in the panel; wheel over it scrolls only the list
    h.frame(Vec2(250, 180), false, false);
    h.frame(Vec2(250, 180), false, false);
    h.frame(Vec2(250, 180), false, false, -120);
    CHECK(h.state.scroll == 0.0f);

    // Below the list, the wheel scrolls the panel
    h.frame(Vec2(250, 350), false, false, -120);
    CHECK(h.state.scroll == UIStyle::SCROLL_STEP);
}

TEST_CASE(ui_child_rows_outside_the_box_cannot_be_clicked) {
    PanelHarness h;
    bool lastClicked = false;
    h.content = [&]() {
        h.ui.beginChild("list", 60.0f);
        for (u32 i = 0; i < 10; ++i) {
            h.ui.pushId(i);
            const bool clicked = h.ui.selectable("Row", false);
            if (i == 9 && clicked) lastClicked = true;
            h.ui.popId();
        }
        h.ui.endChild();
    };

    // Row 9 is laid out far below the 60 px box; clicking where it would be does nothing
    h.frame(Vec2(250, 400), false, false);
    h.drag(Vec2(250, 146 + 9 * 28 + 10), Vec2(250, 146 + 9 * 28 + 10));
    CHECK(!lastClicked);
}

TEST_CASE(ui_segmented_picks_on_release) {
    Harness h;
    i32 index = 0;
    bool changed = false;
    const auto widgets = [&]() { changed = h.ui.segmented("Type", index, { "Point", "Spot", "Directional" }); };

    // Control spans x 116..290 in three 58 px segments; click the third
    h.frame(Vec2(260, 20), false, false, widgets);
    h.frame(Vec2(260, 20), true, false, widgets);
    CHECK(index == 0);
    h.frame(Vec2(260, 20), false, true, widgets);
    CHECK(index == 2);
    CHECK(changed);
}

TEST_CASE(ui_panel_tabs_switch_on_press_and_still_drag) {
    UIContext ui;
    UIDrawList list;
    ui.setDrawList(&list);
    ui.setFont({ FontId::UI, 10.0f, 16.0f });

    UIPanelState state { { 100.0f, 100.0f, 300.0f, 300.0f } };
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
        ui.beginPanel("panel", state, bounds, std::vector<std::string_view> { "Objects", "Lights" });
        ui.endPanel();
        ui.endDraw();
    };

    // "Objects" is 7 x 10 + 28 = 98 px wide starting at x 105, so "Lights" starts at x 203
    frame(Vec2(230, 118), false, false);
    frame(Vec2(230, 118), true, false);
    CHECK(state.activeTab == 1);

    // Keep holding and move: the panel follows
    frame(Vec2(280, 138), true, true);
    CHECK(state.rect.x == 150.0f);
    CHECK(state.rect.y == 120.0f);
    frame(Vec2(280, 138), false, true);
    CHECK(state.activeTab == 1);
}

TEST_CASE(ui_dropdown_picks_from_its_list) {
    Harness h;
    i32 index = 0;
    bool changed = false;
    const auto widgets = [&]() {
        changed = h.ui.dropdown("Preset", h.ui.row(), index, { "Cube", "Plane", "Torus" });
    };

    // Click the button (row y 10..34) to open the list under it
    h.frame(Vec2(100, 20), false, false, widgets);
    h.frame(Vec2(100, 20), true, false, widgets);
    h.frame(Vec2(100, 20), false, true, widgets);
    h.frame(Vec2(100, 20), false, false, widgets);
    CHECK(h.ui.wantsMouse());

    // List rows start at 34 + 2 + 4 = 40, 24 px each; click "Torus"
    const Vec2 torus(100, 40 + 2 * 24 + 12);
    h.frame(torus, false, false, widgets);
    h.frame(torus, true, false, widgets);
    h.frame(torus, false, true, widgets);
    h.frame(torus, false, false, widgets);
    CHECK(index == 2);
}

TEST_CASE(ui_dropdown_closes_on_outside_click_without_passing_it_on) {
    Harness h;
    i32 index = 0;
    const auto widgets = [&]() { h.ui.dropdown("Preset", h.ui.row(), index, { "Cube", "Plane" }); };

    h.frame(Vec2(100, 20), false, false, widgets);
    h.frame(Vec2(100, 20), true, false, widgets);
    h.frame(Vec2(100, 20), false, true, widgets);
    h.frame(Vec2(600, 500), false, false, widgets);

    // Far outside the UI: the press only closes the list, so the viewport doesn't get it
    h.frame(Vec2(600, 500), true, false, widgets);
    CHECK(h.ui.wantsMouse());
    h.frame(Vec2(600, 500), false, true, widgets);
    h.frame(Vec2(600, 500), false, false, widgets);
    CHECK(!h.ui.wantsMouse());
    CHECK(index == 0);
}

TEST_CASE(ui_tree_row_drags_onto_another_row) {
    Harness h;
    bool openA = true, openB = true;
    bool dragging = false, dropped = false, clickedA = false;
    u32 payload = 0;
    Rect rowB;

    const auto widgets = [&]() {
        clickedA = h.ui.treeRow("A", false, {}, 0, true, openA);
        dragging = h.ui.dragSource(7, "A");
        h.ui.treeRow("B", false, {}, 1, false, openB);
        rowB = h.ui.lastItemRect();
        dropped = h.ui.acceptDrop(rowB, payload);
    };

    // Press on A's name, then move past the drag threshold: a drag carrying A's payload starts
    h.frame(Vec2(80, 20), false, false, widgets);
    h.frame(Vec2(80, 20), true, false, widgets);
    CHECK(!dragging);
    h.frame(Vec2(80, 30), true, true, widgets);
    CHECK(dragging && h.ui.isDragging() && h.ui.dragPayload() == 7);

    // Released over B: B takes it, the drag ends, and A wasn't clicked
    h.frame(Vec2(80, 50), true, true, widgets);
    h.frame(Vec2(80, 50), false, true, widgets);
    CHECK(dropped && payload == 7);
    CHECK(!clickedA);
    CHECK(!h.ui.isDragging());
}

TEST_CASE(ui_tree_row_arrow_folds_without_selecting) {
    Harness h;
    bool open = true;
    bool clicked = false;
    const auto widgets = [&]() { clicked = h.ui.treeRow("Parent", false, "2 faces", 0, true, open); };

    // The arrow sits at the start of the row
    h.frame(Vec2(16, 20), false, false, widgets);
    h.frame(Vec2(16, 20), true, false, widgets);
    h.frame(Vec2(16, 20), false, true, widgets);
    CHECK(!open);
    CHECK(!clicked);

    // The rest of the row selects
    h.frame(Vec2(120, 20), true, false, widgets);
    h.frame(Vec2(120, 20), false, true, widgets);
    CHECK(clicked && !open);
}

TEST_CASE(color_conversions_round_trip) {
    f32 h = 0.0f, s = 0.0f, v = 0.0f;
    rgbToHsv(Vec3(1.0f, 0.0f, 0.0f), h, s, v);
    CHECK(h == 0.0f && s == 1.0f && v == 1.0f);
    rgbToHsv(Vec3(0.0f, 0.0f, 1.0f), h, s, v);
    CHECK(std::abs(h - 2.0f / 3.0f) < 1e-5f);
    rgbToHsv(Vec3(0.5f, 0.5f, 0.5f), h, s, v);
    CHECK(s == 0.0f && v == 0.5f);

    bool roundTrips = true;
    for (f32 r = 0.0f; r <= 1.0f; r += 0.25f) {
        for (f32 g = 0.0f; g <= 1.0f; g += 0.25f) {
            for (f32 b = 0.0f; b <= 1.0f; b += 0.25f) {
                rgbToHsv(Vec3(r, g, b), h, s, v);
                const Vec3 back = hsvToRgb(h, s, v);
                roundTrips = roundTrips && std::abs(back.x - r) < 1e-5f && std::abs(back.y - g) < 1e-5f && std::abs(back.z - b) < 1e-5f;
            }
        }
    }
    CHECK(roundTrips);

    CHECK(toHex(Vec3(1.0f, 0.5f, 0.0f)) == "#ff8000");
    Vec3 parsed;
    CHECK(parseHex("#336699", parsed) && std::abs(parsed.y - 0.4f) < 1e-6f);
    CHECK(parseHex("F80", parsed) && parsed.x == 1.0f && std::abs(parsed.y - 0x88 / 255.0f) < 1e-6f && parsed.z == 0.0f);
    parsed = Vec3(0.25f);
    CHECK(!parseHex("#12345", parsed) && !parseHex("#gg0000", parsed) && parsed.x == 0.25f);
}

TEST_CASE(ui_color_picker_square_and_hue_strip) {
    Harness h;
    Vec3 color(0.5f, 0.5f, 0.5f);
    bool activated = false, committed = false;
    const auto widgets = [&]() {
        h.ui.colorEdit("Color", color);
        activated |= h.ui.isItemActivated();
        committed |= h.ui.isItemDeactivatedAfterEdit();
    };

    // Open it by clicking the swatch; that alone isn't an edit
    h.frame(Vec2(200, 20), false, false, widgets);
    h.frame(Vec2(200, 20), true, false, widgets);
    h.frame(Vec2(200, 20), false, true, widgets);
    CHECK(!activated && !committed);

    // The square spans x 116..270 and y 38..158: its top-right corner is full saturation and value, hue red (gray's 0)
    h.frame(Vec2(268, 40), false, false, widgets);
    h.frame(Vec2(268, 40), true, false, widgets);
    CHECK(activated);
    h.frame(Vec2(269, 39), true, true, widgets);
    h.frame(Vec2(269, 39), false, true, widgets);
    CHECK(committed);
    CHECK(color.x > 0.97f && color.y < 0.03f && color.z < 0.03f);

    // The middle of the hue strip (x 274..290) is cyan
    h.frame(Vec2(282, 98), false, false, widgets);
    h.frame(Vec2(282, 98), true, false, widgets);
    h.frame(Vec2(282, 98), false, true, widgets);
    CHECK(color.x < 0.03f && color.y > 0.97f && color.z > 0.97f);
}

TEST_CASE(ui_color_picker_hex_field_commits_one_edit) {
    Harness h;
    Vec3 color(0.5f, 0.5f, 0.5f);
    bool committed = false;
    const auto widgets = [&]() {
        h.ui.colorEdit("Color", color);
        committed |= h.ui.isItemDeactivatedAfterEdit();
    };

    h.frame(Vec2(200, 20), false, false, widgets);
    h.frame(Vec2(200, 20), true, false, widgets);
    h.frame(Vec2(200, 20), false, true, widgets);

    // The hex field is the row under the square (y 162..186): click it, type, Enter
    h.frame(Vec2(200, 174), false, false, widgets);
    h.frame(Vec2(200, 174), true, false, widgets);
    h.frame(Vec2(200, 174), false, true, widgets);
    h.typed = "#336699";
    h.frame(Vec2(200, 174), false, false, widgets);
    h.keys = { UIKey::Enter };
    h.frame(Vec2(200, 174), false, false, widgets);

    CHECK(committed);
    CHECK(std::abs(color.x - 0.2f) < 1e-6f && std::abs(color.y - 0.4f) < 1e-6f && std::abs(color.z - 0.6f) < 1e-6f);
}
