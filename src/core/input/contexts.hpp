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
    InputContext_Rotate          = 1 << 8
};