#pragma once

#include "core/input/action_map.hpp"

// A small program's actions and contexts, for the input tests
enum class Action : u8 {
    Quit,
    EnterCommand,
    ConsoleBackspace,
    Select,
    GrabSelection,
    ScaleSelection,
    FaceMode,
    FrameSelected,
    XAxis,
    AxisFree,
    Undo,
    Redo,
    SaveProject,
    SaveProjectAs,
    ModalConfirm
};

enum InputContext : u32 {
    InputContext_Global          = 1 << 0,
    InputContext_Console         = 1 << 1,
    InputContext_SelectionVertex = 1 << 2,
    InputContext_SelectionFace   = 1 << 3,
    InputContext_Grab            = 1 << 4,
    InputContext_Bevel           = 1 << 5,
    InputContext_Modal           = 1 << 6
};

constexpr u32 InputContext_AnySelection = InputContext_SelectionVertex | InputContext_SelectionFace;

// Global is always on, and vertex and face are modes
inline ContextManager testContexts() {
    return ContextManager(InputContext_Global, { InputContext_SelectionVertex, InputContext_SelectionFace });
}

// A modal window, then the console, owns the input, and Quit gets past the keyboard block
inline ActionMap testActions() {
    ActionMap actions;
    actions.setOwnerContexts({ InputContext_Modal, InputContext_Console });
    actions.setUnblockable(Action::Quit);
    return actions;
}
