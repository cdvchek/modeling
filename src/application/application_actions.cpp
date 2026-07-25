#include "application/application.hpp"

void Application::registerDefaultActions(AppContext& ctx) {
    auto& actions = ctx.systems.actions;

    actions.subscribe(Action::Quit,            DefaultKeybinds::Quit,            InputContext_Global);
    actions.subscribe(Action::ViewportOrbit,   DefaultKeybinds::ViewportOrbit,   InputContext_Selection);
    actions.subscribe(Action::ViewportPan,     DefaultKeybinds::ViewportPan,     InputContext_Selection);
    actions.subscribe(Action::ViewportZoom,    DefaultKeybinds::ViewportZoom,    InputContext_Selection);
    actions.subscribe(Action::Select,          DefaultKeybinds::Select,          InputContext_Selection);
    actions.subscribe(Action::AddSelection,    DefaultKeybinds::AddSelection,    InputContext_Selection);
    actions.subscribe(Action::RemoveSelection, DefaultKeybinds::RemoveSelection, InputContext_Selection);
    actions.subscribe(Action::GrabSelection,   DefaultKeybinds::GrabSelection,   InputContext_Selection);
    actions.subscribe(Action::ConfirmGrab,     DefaultKeybinds::ConfirmGrab,     InputContext_Grab);
    actions.subscribe(Action::CancelGrab,      DefaultKeybinds::CancelGrab,      InputContext_Grab);
    actions.subscribe(Action::ScaleSelection,  DefaultKeybinds::ScaleSelection,  InputContext_Selection);
    actions.subscribe(Action::ConfirmScale,    DefaultKeybinds::ConfirmScale,    InputContext_Scale);
    actions.subscribe(Action::CancelScale,     DefaultKeybinds::CancelScale,     InputContext_Scale);
    // actions.subscribe(Action::DeleteSelection, DefaultKeybinds::DeleteSelection);
    // actions.subscribe(Action::Duplicate, DefaultKeybinds::Duplicate);
    // actions.subscribe(Action::Connect, DefaultKeybinds::Connect);
    // actions.subscribe(Action::Disconnect, DefaultKeybinds::Disconnect);
    // actions.subscribe(Action::Save, DefaultKeybinds::Save);
    // actions.subscribe(Action::Undo, DefaultKeybinds::Undo);
    // actions.subscribe(Action::Redo, DefaultKeybinds::Redo);
}