#include "application/application.hpp"

void Application::registerDefaultActions(AppContext& ctx) {
    auto& actions = ctx.systems.actions;

    actions.subscribe(Action::Quit,             DefaultKeybinds::Quit,             InputContext_Global);
    actions.subscribe(Action::ViewportOrbit,    DefaultKeybinds::ViewportOrbit,    InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ViewportPan,      DefaultKeybinds::ViewportPan,      InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ViewportZoom,     DefaultKeybinds::ViewportZoom,     InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::VertexMode,       DefaultKeybinds::VertexMode,       InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::EdgeMode,         DefaultKeybinds::EdgeMode,         InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::FaceMode,         DefaultKeybinds::FaceMode,         InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::Select,           DefaultKeybinds::Select,           InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::AddSelection,     DefaultKeybinds::AddSelection,     InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::RemoveSelection,  DefaultKeybinds::RemoveSelection,  InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::GrabSelection,    DefaultKeybinds::GrabSelection,    InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ConfirmGrab,      DefaultKeybinds::ConfirmGrab,      InputContext_Grab);
    actions.subscribe(Action::CancelGrab,       DefaultKeybinds::CancelGrab,       InputContext_Grab);
    actions.subscribe(Action::ScaleSelection,   DefaultKeybinds::ScaleSelection,   InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ConfirmScale,     DefaultKeybinds::ConfirmScale,     InputContext_Scale);
    actions.subscribe(Action::CancelScale,      DefaultKeybinds::CancelScale,      InputContext_Scale);
    actions.subscribe(Action::ExtrudeSelection, DefaultKeybinds::ExtrudeSelection, InputContext_SelectionFace);
    actions.subscribe(Action::InsetSelection,   DefaultKeybinds::InsetSelection,   InputContext_SelectionFace);
    // actions.subscribe(Action::DeleteSelection, DefaultKeybinds::DeleteSelection);
    // actions.subscribe(Action::Duplicate, DefaultKeybinds::Duplicate);
    // actions.subscribe(Action::Connect, DefaultKeybinds::Connect);
    // actions.subscribe(Action::Disconnect, DefaultKeybinds::Disconnect);
    // actions.subscribe(Action::Save, DefaultKeybinds::Save);
    // actions.subscribe(Action::Undo, DefaultKeybinds::Undo);
    // actions.subscribe(Action::Redo, DefaultKeybinds::Redo);
}