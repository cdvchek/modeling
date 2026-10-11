#pragma once

#include "core/console/console.hpp"
#include "core/input/action_map.hpp"

// A program's actions for the console's keys
struct ConsoleActions {
    ActionId enter;
    ActionId backspace;
    ActionId remove;
    ActionId cursorLeft;
    ActionId cursorRight;
    ActionId historyOlder;
    ActionId historyNewer;
};

// Runs the console's editing keys for a frame; they repeat while held (after the OS repeat delay), Enter doesn't
void updateConsoleInput(Console& console, CommandSystem& commands, const ConsoleActions& keys,
                        const ActionMap& actions, const InputState& input, u32 context);