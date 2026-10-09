#include "test.hpp"
#include "core/input/action_map.hpp"

TEST_CASE(keybind_label_joins_inputs) {
    CHECK(keybindLabel({ { key(Key::LeftCtrl), key(Key::Z) } }) == "Ctrl+Z");
    CHECK(keybindLabel({ { key(Key::M), key(Key::V) } }) == "M+V");
    CHECK(keybindLabel({ { key(Key::Delete) } }) == "Del");
    CHECK(keybindLabel({ { mouse(MouseButton::B4) } }) == "Mouse4");
    CHECK(keybindLabel({ { key(Key::Num3) } }) == "3");
}

TEST_CASE(action_dispatch_runs_pressed_handlers_in_context) {
    ActionMap actions;
    ContextManager contexts;
    InputState input;
    i32 runs = 0;

    actions.subscribe(Action::GrabSelection, { { key(Key::G) } }, InputContext_SelectionVertex);
    actions.setHandler(Action::GrabSelection, { "Grab", {}, [&runs] { ++runs; } });

    input.onKey(static_cast<u16>(Key::G), true);
    actions.dispatch(input, contexts);
    CHECK(runs == 1);

    // Held, not newly pressed: nothing
    input.beginFrame();
    actions.dispatch(input, contexts);
    CHECK(runs == 1);

    // Pressed outside its context: nothing
    input.onKey(static_cast<u16>(Key::G), false);
    input.beginFrame();
    contexts.setSelectionContext(InputContext_SelectionFace);
    input.onKey(static_cast<u16>(Key::G), true);
    actions.dispatch(input, contexts);
    CHECK(runs == 1);
}

TEST_CASE(action_dispatch_skips_handlers_that_cannot_run) {
    ActionMap actions;
    ContextManager contexts;
    InputState input;
    bool allowed = false;
    i32 runs = 0;

    actions.subscribe(Action::Undo, { { key(Key::Z) } }, InputContext_Global);
    actions.setHandler(Action::Undo, { "Undo", [&allowed] { return allowed; }, [&runs] { ++runs; } });

    input.onKey(static_cast<u16>(Key::Z), true);
    actions.dispatch(input, contexts);
    CHECK(runs == 0);

    allowed = true;
    input.onKey(static_cast<u16>(Key::Z), false);
    input.beginFrame();
    input.onKey(static_cast<u16>(Key::Z), true);
    actions.dispatch(input, contexts);
    CHECK(runs == 1);
}

TEST_CASE(action_availability_checks_binding_context_and_can_run) {
    ActionMap actions;

    actions.subscribe(Action::XAxis, { { key(Key::X) } }, InputContext_Grab);
    actions.setHandler(Action::XAxis, { "X", {}, [] {} });
    actions.setHandler(Action::AxisFree, { "Free", [] { return true; }, [] {} });

    CHECK(actions.isAvailable(Action::XAxis, InputContext_Global | InputContext_Grab));
    CHECK(!actions.isAvailable(Action::XAxis, InputContext_Global | InputContext_Bevel));

    // Unbound actions depend only on canRun
    CHECK(actions.isAvailable(Action::AxisFree, InputContext_Global));
    CHECK(!actions.isAvailable(Action::Redo, InputContext_Global));
    CHECK(actions.getKeybind(Action::AxisFree) == nullptr);
    CHECK(actions.getKeybind(Action::XAxis) != nullptr);
}

TEST_CASE(action_modifiers_keep_shortcuts_apart) {
    ActionMap actions;
    ContextManager contexts;
    InputState input;
    std::string ran;

    actions.subscribe(Action::ScaleSelection, { { key(Key::S) } }, InputContext_SelectionVertex);
    actions.subscribe(Action::SaveProject, { { key(Key::LeftCtrl), key(Key::S) } }, InputContext_SelectionVertex);
    actions.subscribe(Action::SaveProjectAs, { { key(Key::LeftCtrl), key(Key::LeftShift), key(Key::S) } }, InputContext_SelectionVertex);
    actions.setHandler(Action::ScaleSelection, { "Scale", {}, [&ran] { ran += "scale "; } });
    actions.setHandler(Action::SaveProject, { "Save", {}, [&ran] { ran += "save "; } });
    actions.setHandler(Action::SaveProjectAs, { "Save As", {}, [&ran] { ran += "saveas "; } });

    auto press = [&](std::initializer_list<Key> keys) {
        input.beginFrame();
        for (Key k : { Key::LeftCtrl, Key::RightCtrl, Key::LeftShift, Key::S }) input.onKey(static_cast<u16>(k), false);
        input.beginFrame();
        for (Key k : keys) input.onKey(static_cast<u16>(k), true);
        ran.clear();
        actions.dispatch(input, contexts);
        return ran;
    };

    CHECK(press({ Key::S }) == "scale ");
    CHECK(press({ Key::LeftCtrl, Key::S }) == "save ");
    CHECK(press({ Key::LeftCtrl, Key::LeftShift, Key::S }) == "saveas ");
    // Right Ctrl doesn't match the left-Ctrl binding, but still keeps S from scaling
    CHECK(press({ Key::RightCtrl, Key::S }) == "");
    // Shift alone doesn't stop a plain key
    CHECK(press({ Key::LeftShift, Key::S }) == "scale ");
}

TEST_CASE(action_modal_window_owns_the_keys) {
    ActionMap actions;
    ContextManager contexts;
    InputState input;
    std::string ran;

    actions.subscribe(Action::GrabSelection, { { key(Key::G) } }, InputContext_SelectionVertex);
    actions.subscribe(Action::ModalConfirm, { { key(Key::Enter) } }, InputContext_Modal);
    actions.subscribe(Action::EnterCommand, { { key(Key::Enter) } }, InputContext_Console);
    actions.setHandler(Action::GrabSelection, { "Grab", {}, [&ran] { ran += "grab "; } });
    actions.setHandler(Action::ModalConfirm, { "OK", {}, [&ran] { ran += "ok "; } });
    actions.setHandler(Action::EnterCommand, { "Enter", {}, [&ran] { ran += "command "; } });

    auto press = [&](Key k) {
        input.beginFrame();
        input.onKey(static_cast<u16>(Key::G), false);
        input.onKey(static_cast<u16>(Key::Enter), false);
        input.beginFrame();
        input.onKey(static_cast<u16>(k), true);
        ran.clear();
        actions.dispatch(input, contexts);
        return ran;
    };

    CHECK(press(Key::G) == "grab ");
    CHECK(press(Key::Enter) == "");

    // With a modal window open, only its bindings work, even over the console
    contexts.addContext(InputContext_Modal);
    contexts.addContext(InputContext_Console);
    CHECK(press(Key::G) == "");
    CHECK(press(Key::Enter) == "ok ");

    contexts.removeContext(InputContext_Modal);
    CHECK(press(Key::Enter) == "command ");
}

TEST_CASE(input_a_click_within_one_frame_still_counts) {
    InputState input;
    const u16 left = static_cast<u16>(MouseButton::Left);

    // Down and up between two frames: both are reported, though the button isn't down any more
    input.beginFrame();
    input.onMouseButton(left, true);
    input.onMouseButton(left, false);
    CHECK(input.wasMousePressedThisFrame(left));
    CHECK(input.wasMouseReleasedThisFrame(left));
    CHECK(!input.isMouseDown(left));

    ActionMap actions;
    ContextManager contexts;
    i32 runs = 0;
    actions.subscribe(Action::Select, { { mouse(MouseButton::Left) } }, InputContext_SelectionVertex);
    actions.setHandler(Action::Select, { "Select", {}, [&runs] { ++runs; } });
    actions.dispatch(input, contexts);
    CHECK(runs == 1);

    // Next frame nothing new happened
    input.beginFrame();
    CHECK(!input.wasMousePressedThisFrame(left) && !input.wasMouseReleasedThisFrame(left));
    actions.dispatch(input, contexts);
    CHECK(runs == 1);
}

TEST_CASE(input_mouse_moves_add_up_within_a_frame) {
    InputState input;
    input.onMouseMove(10, 10);
    input.beginFrame();

    input.onMouseMove(15, 12);
    input.onMouseMove(20, 18);
    // A click at the same spot reports its position without changing the movement
    input.onMouseMove(20, 18);
    CHECK(input.getMouseDeltaX() == 10 && input.getMouseDeltaY() == 8);
    CHECK(input.getMouseX() == 20 && input.getMouseY() == 18);

    input.beginFrame();
    CHECK(input.getMouseDeltaX() == 0 && input.getMouseDeltaY() == 0);
}

TEST_CASE(action_filter_switches_actions_off) {
    ActionMap actions;
    ContextManager contexts;
    InputState input;
    i32 runs = 0;

    actions.subscribe(Action::GrabSelection, { { key(Key::G) } }, InputContext_SelectionVertex);
    actions.setHandler(Action::GrabSelection, { "Grab", {}, [&runs] { ++runs; } });

    // Turned down by the filter: the key doesn't fire it, and a menu can't run it
    bool allowGrab = false;
    actions.setFilter([&allowGrab](Action action) { return action != Action::GrabSelection || allowGrab; });
    input.onKey(static_cast<u16>(Key::G), true);
    CHECK(!actions.wasActionPressedThisFrame(Action::GrabSelection, input, contexts.getContext()));
    actions.dispatch(input, contexts);
    CHECK(runs == 0);
    CHECK(!actions.canRun(Action::GrabSelection));
    CHECK(!actions.isAvailable(Action::GrabSelection, contexts.getContext()));

    // Allowed again, everything works as before
    allowGrab = true;
    CHECK(actions.isAvailable(Action::GrabSelection, contexts.getContext()));
    actions.dispatch(input, contexts);
    CHECK(runs == 1);
}

TEST_CASE(a_chord_hides_its_own_single_keys) {
    ActionMap actions;
    ContextManager contexts;
    InputState input;
    i32 faceMode = 0, frame = 0;

    actions.subscribe(Action::FaceMode, { { key(Key::M), key(Key::F) } }, InputContext_AnySelection);
    actions.subscribe(Action::FrameSelected, { { key(Key::F) } }, InputContext_AnySelection);
    actions.setHandler(Action::FaceMode, { "Face", {}, [&faceMode] { ++faceMode; } });
    actions.setHandler(Action::FrameSelected, { "Frame", {}, [&frame] { ++frame; } });

    // M+F is face mode alone, not face mode and F
    input.onKey(static_cast<u16>(Key::M), true);
    input.onKey(static_cast<u16>(Key::F), true);
    actions.dispatch(input, contexts);
    CHECK(faceMode == 1);
    CHECK(frame == 0);

    // F on its own still works
    input.onKey(static_cast<u16>(Key::M), false);
    input.onKey(static_cast<u16>(Key::F), false);
    input.beginFrame();
    input.onKey(static_cast<u16>(Key::F), true);
    actions.dispatch(input, contexts);
    CHECK(frame == 1);
    CHECK(faceMode == 1);
}
