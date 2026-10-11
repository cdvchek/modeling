#pragma once

#include <string>
#include <types>
#include <vector>
#include "core/math/vec2.hpp"

class InputState;

// Editing keys a text field understands; held keys repeat (the app passes the OS's key repeats through)
enum class UIKey : u8 {
    Left,
    Right,
    Home,
    End,
    Backspace,
    Delete,
    Enter,
    Escape,
    SelectAll,   // Ctrl+A
    Copy,        // Ctrl+C
    Cut,         // Ctrl+X
    Paste        // Ctrl+V
};

// Mouse and keyboard state for one frame, filled by the application from its input system
struct UIInput {
    static constexpr u32 LEFT = 0;
    static constexpr u32 RIGHT = 1;
    static constexpr u32 MIDDLE = 2;
    static constexpr u32 BUTTON_COUNT = 3;

    Vec2 mouse;
    Vec2 mouseDelta;
    bool down[BUTTON_COUNT] = {};
    bool pressed[BUTTON_COUNT] = {};
    bool released[BUTTON_COUNT] = {};
    i32 scroll = 0;

    std::string text;            // printable characters typed this frame
    std::vector<UIKey> keys;     // editing keys pressed this frame, in order
    bool shift = false;          // held: arrow keys, Home, and End extend the selection
    f64 time = 0.0;              // seconds, for the caret blink

    bool anyDown() const { return down[LEFT] || down[RIGHT] || down[MIDDLE]; }
    bool anyPressed() const { return pressed[LEFT] || pressed[RIGHT] || pressed[MIDDLE]; }
};

// This frame's mouse, typed text, and editing keys from the input system, stamped with the current time
UIInput makeUIInput(const InputState& input);
