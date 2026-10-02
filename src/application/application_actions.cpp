#include "application/application.hpp"

void Application::registerDefaultActions(AppContext& ctx) {
    auto& actions = ctx.systems.actions;

    actions.subscribe(Action::Quit,                DefaultKeybinds::Quit,                InputContext_Global);
    actions.subscribe(Action::ToggleConsole,       DefaultKeybinds::ToggleConsole,       InputContext_Global | InputContext_Console);
    actions.subscribe(Action::EnterCommand,        DefaultKeybinds::EnterCommand,        InputContext_Console);
    actions.subscribe(Action::ConsoleBackspace,    DefaultKeybinds::ConsoleBackspace,    InputContext_Console);
    actions.subscribe(Action::ConsoleDelete,       DefaultKeybinds::ConsoleDelete,       InputContext_Console);
    actions.subscribe(Action::ConsoleCursorLeft,   DefaultKeybinds::ConsoleCursorLeft,   InputContext_Console);
    actions.subscribe(Action::ConsoleCursorRight,  DefaultKeybinds::ConsoleCursorRight,  InputContext_Console);
    actions.subscribe(Action::ConsoleHistoryOlder, DefaultKeybinds::ConsoleHistoryOlder, InputContext_Console);
    actions.subscribe(Action::ConsoleHistoryNewer, DefaultKeybinds::ConsoleHistoryNewer, InputContext_Console);
    actions.subscribe(Action::ViewportOrbit,       DefaultKeybinds::ViewportOrbit,       InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ViewportPan,         DefaultKeybinds::ViewportPan,         InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ViewportZoom,        DefaultKeybinds::ViewportZoom,        InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::VertexMode,          DefaultKeybinds::VertexMode,          InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::EdgeMode,            DefaultKeybinds::EdgeMode,            InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::FaceMode,            DefaultKeybinds::FaceMode,            InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::Select,              DefaultKeybinds::Select,              InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::AddSelection,        DefaultKeybinds::AddSelection,        InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::RemoveSelection,     DefaultKeybinds::RemoveSelection,     InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::GrabSelection,       DefaultKeybinds::GrabSelection,       InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ConfirmGrab,         DefaultKeybinds::ConfirmGrab,         InputContext_Grab);
    actions.subscribe(Action::CancelGrab,          DefaultKeybinds::CancelGrab,          InputContext_Grab);
    actions.subscribe(Action::ScaleSelection,      DefaultKeybinds::ScaleSelection,      InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::ConfirmScale,        DefaultKeybinds::ConfirmScale,        InputContext_Scale);
    actions.subscribe(Action::CancelScale,         DefaultKeybinds::CancelScale,         InputContext_Scale);
    actions.subscribe(Action::ExtrudeSelection,    DefaultKeybinds::ExtrudeSelection,    InputContext_SelectionFace);
    actions.subscribe(Action::InsetSelection,      DefaultKeybinds::InsetSelection,      InputContext_SelectionFace);
    actions.subscribe(Action::DeleteSelection,     DefaultKeybinds::DeleteSelection,     InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::FillFaceLoop,        DefaultKeybinds::FillFaceLoop,        InputContext_SelectionEdge);
    actions.subscribe(Action::ConnectVertices,     DefaultKeybinds::ConnectVertices,     InputContext_SelectionVertex);
    actions.subscribe(Action::XAxis,               DefaultKeybinds::XAxis,               InputContext_Grab | InputContext_Scale | InputContext_Rotate);
    actions.subscribe(Action::YAxis,               DefaultKeybinds::YAxis,               InputContext_Grab | InputContext_Scale | InputContext_Rotate);
    actions.subscribe(Action::ZAxis,               DefaultKeybinds::ZAxis,               InputContext_Grab | InputContext_Scale | InputContext_Rotate);
    actions.subscribe(Action::RotateSelection,     DefaultKeybinds::RotateSelection,     InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::RotateConfirm,       DefaultKeybinds::RotateConfirm,       InputContext_Rotate);
    actions.subscribe(Action::RotateCancel,        DefaultKeybinds::RotateCancel,        InputContext_Rotate);
}