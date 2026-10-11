#pragma once

#include <types>

#include "core/input/context_manager.hpp"

enum InputContext : u32 {
    InputContext_None    = 0,
    InputContext_Global  = 1 << 0,
    InputContext_Console = 1 << 1,
    InputContext_Editor  = 1 << 2
};

// Global is always on, and the editor is the one mode so far
inline ContextManager makeInputContexts() {
    return ContextManager(InputContext_Global, { InputContext_Editor });
}
