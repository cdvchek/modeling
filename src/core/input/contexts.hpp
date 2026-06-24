#pragma once

#include <types>

enum InputContext : u32 {
    InputContext_None      = 0,
    InputContext_Global    = 1 << 0,
    InputContext_Selection = 1 << 1,
    InputContext_Grab      = 1 << 2
};