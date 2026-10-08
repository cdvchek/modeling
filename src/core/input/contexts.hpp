#pragma once

#include <types>

enum InputContext : u32 {
    InputContext_None            = 0,
    InputContext_Global          = 1 << 0,
    InputContext_Console         = 1 << 1,
    InputContext_Debug           = 1 << 2,
    InputContext_SelectionVertex = 1 << 3,
    InputContext_SelectionEdge   = 1 << 4,
    InputContext_SelectionFace   = 1 << 5,
    InputContext_Grab            = 1 << 6,
    InputContext_Scale           = 1 << 7,
    InputContext_Rotate          = 1 << 8,
    InputContext_XAxis           = 1 << 9,
    InputContext_YAxis           = 1 << 10,
    InputContext_ZAxis           = 1 << 11,
    InputContext_Bevel           = 1 << 12,
    InputContext_SelectionObject = 1 << 13,
    InputContext_Inset           = 1 << 14,
    InputContext_Modal           = 1 << 15   // a modal window is open; only its bindings work
};

// Vertex, edge, and face mode edit the active object's mesh
constexpr u32 InputContext_EditModes = InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace;
// Any selection mode, object mode included
constexpr u32 InputContext_AnySelection = InputContext_EditModes | InputContext_SelectionObject;