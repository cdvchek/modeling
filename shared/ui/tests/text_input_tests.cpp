#include "test.hpp"
#include "core/tests/test_input.hpp"
#include "ui/ui_context.hpp"

#include <functional>

namespace {
    // The first row's control column runs from x 116 to 290, y 10 to 34 (see ui_context_tests.cpp)
    const Rect REGION { 0.0f, 0.0f, 300.0f, 400.0f };
    const Vec2 FIELD(200.0f, 20.0f);
    const Vec2 X_BOX(130.0f, 20.0f);
    const Vec2 OUTSIDE(500.0f, 300.0f);

    struct Harness {
        UIContext ui;
        UIDrawList list;
        Vec2 lastMouse;
        bool wasDown = false;

        Harness() {
            ui.setDrawList(&list);
            ui.setFont({ FontId::UI, 10.0f, 16.0f });
        }

        void frame(Vec2 mouse, bool down, const std::function<void()>& widgets, const std::string& typed = {}, const std::vector<UIKey>& keys = {}) {
            UIInput input;
            input.mouse = mouse;
            input.mouseDelta = mouse - lastMouse;
            input.down[UIInput::LEFT] = down;
            input.pressed[UIInput::LEFT] = down && !wasDown;
            input.released[UIInput::LEFT] = !down && wasDown;
            input.text = typed;
            input.keys = keys;
            lastMouse = mouse;
            wasDown = down;

            ui.beginFrame(input, true);
            list.clear();
            ui.beginDraw();
            ui.beginRegion(REGION);
            widgets();
            ui.endRegion();
            ui.endDraw();
        }

        void click(Vec2 at, const std::function<void()>& widgets) {
            frame(at, false, widgets);
            frame(at, true, widgets);
            frame(at, false, widgets);
        }
    };
}

TEST_CASE(ui_text_field_replaces_and_commits_on_enter) {
    Harness h;
    std::string name = "Cube";
    bool changed = false;
    bool committedEdit = false;
    const auto widgets = [&]() {
        changed = h.ui.textField("Name", name);
        committedEdit = h.ui.isItemDeactivatedAfterEdit();
    };

    h.click(FIELD, widgets);
    CHECK(h.ui.wantsKeyboard());

    // Everything is selected when editing starts, so typing replaces it
    h.frame(FIELD, false, widgets, "Box");
    CHECK(!changed);
    CHECK(name == "Cube");

    h.frame(FIELD, false, widgets, {}, { UIKey::Enter });
    CHECK(changed);
    CHECK(committedEdit);
    CHECK(name == "Box");
    CHECK(!h.ui.wantsKeyboard());
}

TEST_CASE(ui_text_field_escape_restores) {
    Harness h;
    std::string name = "Cube";
    bool changed = false;
    const auto widgets = [&]() { changed = h.ui.textField("Name", name); };

    h.click(FIELD, widgets);
    h.frame(FIELD, false, widgets, "Box");
    h.frame(FIELD, false, widgets, {}, { UIKey::Escape });

    CHECK(!changed);
    CHECK(name == "Cube");
    CHECK(!h.ui.wantsKeyboard());
}

TEST_CASE(ui_text_field_click_outside_commits_and_keeps_the_click) {
    Harness h;
    std::string name = "Light 1";
    bool changed = false;
    const auto widgets = [&]() { changed = h.ui.textField("Name", name); };

    h.click(FIELD, widgets);
    h.frame(FIELD, false, widgets, "Lamp");

    // The press far outside commits, and the viewport doesn't also get it
    h.frame(OUTSIDE, true, widgets);
    CHECK(changed);
    CHECK(name == "Lamp");
    CHECK(h.ui.wantsMouse());
}

TEST_CASE(ui_text_field_editing_keys) {
    Harness h;
    std::string name = "abc";
    const auto widgets = [&]() { h.ui.textField("Name", name); };

    h.click(FIELD, widgets);

    // Right collapses the selection at the end; Backspace, Home, Delete, then typing at the caret
    h.frame(FIELD, false, widgets, {}, { UIKey::Right, UIKey::Backspace, UIKey::Home, UIKey::Delete });
    h.frame(FIELD, false, widgets, "x");
    h.frame(FIELD, false, widgets, {}, { UIKey::Enter });

    CHECK(name == "xb");
}

TEST_CASE(ui_text_field_drops_an_empty_edit) {
    Harness h;
    std::string name = "Cube";
    bool changed = false;
    const auto widgets = [&]() { changed = h.ui.textField("Name", name); };

    h.click(FIELD, widgets);
    h.frame(FIELD, false, widgets, {}, { UIKey::Backspace, UIKey::Enter });

    CHECK(!changed);
    CHECK(name == "Cube");
}

TEST_CASE(ui_text_field_clipboard) {
    Harness h;
    std::string clipboard;
    h.ui.setClipboard([&]() { return clipboard; }, [&](const std::string& text) { clipboard = text; });

    std::string name = "abc";
    const auto widgets = [&]() { h.ui.textField("Name", name); };

    h.click(FIELD, widgets);
    h.frame(FIELD, false, widgets, {}, { UIKey::Copy });
    CHECK(clipboard == "abc");

    h.frame(FIELD, false, widgets, {}, { UIKey::Cut, UIKey::Paste, UIKey::Paste, UIKey::Enter });
    CHECK(name == "abcabc");

    // Pasted text keeps one line of drawable characters
    clipboard = "one\ntwo";
    h.click(FIELD, widgets);
    h.frame(FIELD, false, widgets, {}, { UIKey::Paste, UIKey::Enter });
    CHECK(name == "one");
}

TEST_CASE(ui_text_field_edit_ends_when_not_drawn) {
    Harness h;
    std::string name = "Cube";
    const auto widgets = [&]() { h.ui.textField("Name", name); };

    h.click(FIELD, widgets);
    CHECK(h.ui.wantsKeyboard());

    h.frame(FIELD, false, [] {});
    CHECK(!h.ui.wantsKeyboard());
}

TEST_CASE(ui_drag_float3_click_types_a_value) {
    Harness h;
    Vec3 value(1.0f, 2.0f, 3.0f);
    bool changed = false;
    bool committedEdit = false;
    const auto widgets = [&]() {
        changed = h.ui.dragFloat3("Position", value, 0.01f);
        committedEdit = h.ui.isItemDeactivatedAfterEdit();
    };

    // A press and release without moving opens X for typing, with its value selected
    h.click(X_BOX, widgets);
    CHECK(h.ui.wantsKeyboard());
    CHECK(value.x == 1.0f);

    h.frame(X_BOX, false, widgets, "2.5");
    h.frame(X_BOX, false, widgets, {}, { UIKey::Enter });

    CHECK(changed);
    CHECK(committedEdit);
    CHECK(value.x == 2.5f);
    CHECK(value.y == 2.0f);
}

TEST_CASE(ui_drag_float3_ignores_jitter_and_bad_numbers) {
    Harness h;
    Vec3 value(1.0f, 2.0f, 3.0f);
    const auto widgets = [&]() { h.ui.dragFloat3("Position", value, 0.01f); };

    // A 2 px wobble during the click is still a click: no drag, and it opens for typing
    h.frame(X_BOX, false, widgets);
    h.frame(X_BOX, true, widgets);
    h.frame(X_BOX + Vec2(2.0f, 0.0f), true, widgets);
    h.frame(X_BOX + Vec2(2.0f, 0.0f), false, widgets);
    CHECK(value.x == 1.0f);
    CHECK(h.ui.wantsKeyboard());

    // Text that isn't a number leaves the value alone
    h.frame(X_BOX, false, widgets, "abc");
    h.frame(X_BOX, false, widgets, {}, { UIKey::Enter });
    CHECK(value.x == 1.0f);
}

TEST_CASE(action_keyboard_block_spares_mouse_and_quit) {
    ActionMap actions = testActions();
    ContextManager contexts = testContexts();
    InputState input;

    actions.subscribe(Action::GrabSelection, { { key(Key::G) } }, InputContext_Global);
    actions.subscribe(Action::Quit, { { key(Key::LeftAlt), key(Key::F4) } }, InputContext_Global);
    actions.subscribe(Action::Select, { { mouse(MouseButton::Left) } }, InputContext_Global);
    actions.setKeyboardBlocked(true);

    input.onKey(static_cast<u16>(Key::G), true);
    input.onKey(static_cast<u16>(Key::LeftAlt), true);
    input.onKey(static_cast<u16>(Key::F4), true);
    input.onMouseButton(static_cast<u16>(MouseButton::Left), true);

    CHECK(!actions.wasActionPressedThisFrame(Action::GrabSelection, input, contexts.getContext()));
    CHECK(actions.wasActionPressedThisFrame(Action::Quit, input, contexts.getContext()));
    CHECK(actions.wasActionPressedThisFrame(Action::Select, input, contexts.getContext()));
}

TEST_CASE(action_repeats_follow_os_key_repeats) {
    ActionMap actions = testActions();
    ContextManager contexts = testContexts();
    InputState input;
    const u16 backspace = static_cast<u16>(Key::Backspace);
    actions.subscribe(Action::ConsoleBackspace, { { key(Key::Backspace) } }, InputContext_Global);

    input.onKey(backspace, true);
    CHECK(actions.wasActionPressedOrRepeated(Action::ConsoleBackspace, input, contexts.getContext()));

    // Held, no repeat yet (the OS waits before repeating): nothing
    input.beginFrame();
    CHECK(!actions.wasActionPressedOrRepeated(Action::ConsoleBackspace, input, contexts.getContext()));

    // The OS sends a repeat while the key is still down
    input.beginFrame();
    input.onKey(backspace, true);
    CHECK(!actions.wasActionPressedThisFrame(Action::ConsoleBackspace, input, contexts.getContext()));
    CHECK(actions.wasActionPressedOrRepeated(Action::ConsoleBackspace, input, contexts.getContext()));
}

TEST_CASE(input_typed_text_skips_control_characters) {
    InputState input;
    for (char c : std::string("a\bB\r\t c")) input.onChar(c);
    input.onChar(static_cast<char>(1));   // Ctrl+A
    CHECK(input.getTypedText() == "aB c");

    input.beginFrame();
    CHECK(input.getTypedText().empty());
}
