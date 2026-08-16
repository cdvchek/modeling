#pragma once

#include <types>

enum InputContext : u32 {
    InputContext_None            = 0,
    InputContext_Global          = 1 << 0,
    InputContext_SelectionVertex = 1 << 1,
    InputContext_SelectionEdge   = 1 << 2,
    InputContext_SelectionFace   = 1 << 3,
    InputContext_Grab            = 1 << 4,
    InputContext_Scale           = 1 << 5,
    InputContext_Extrude         = 1 << 6,
};