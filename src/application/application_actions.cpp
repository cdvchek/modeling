#include "application/application.hpp"

void Application::registerDefaultActions(AppContext& ctx) {
    auto& actions = ctx.systems.actions;

    actions.subscribe(Action::Quit, DefaultKeybinds::Quit);
    actions.subscribe(Action::ViewportOrbit, DefaultKeybinds::ViewportOrbit);
    actions.subscribe(Action::ViewportPan, DefaultKeybinds::ViewportPan);
    actions.subscribe(Action::ViewportZoom, DefaultKeybinds::ViewportZoom);
    actions.subscribe(Action::Select, DefaultKeybinds::Select);
    actions.subscribe(Action::Insert, DefaultKeybinds::Insert);
    actions.subscribe(Action::DeleteSelection, DefaultKeybinds::DeleteSelection);
    actions.subscribe(Action::Duplicate, DefaultKeybinds::Duplicate);
    actions.subscribe(Action::Connect, DefaultKeybinds::Connect);
    actions.subscribe(Action::Disconnect, DefaultKeybinds::Disconnect);
    actions.subscribe(Action::Save, DefaultKeybinds::Save);
    actions.subscribe(Action::Undo, DefaultKeybinds::Undo);
    actions.subscribe(Action::Redo, DefaultKeybinds::Redo);
}