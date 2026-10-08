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
