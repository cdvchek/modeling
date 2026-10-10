#pragma once

#include <types>
#include <string>
#include <vector>
#include "core/input/keys.hpp"

enum class InputKind : u8 {
    Key,
    MouseButton,
    Axis
};

struct Input {
    InputKind kind;
    u16 code;
};

struct Keybind {
    std::vector<Input> inputs;
};

inline Input key(Key key) {
    return Input{
        InputKind::Key,
        static_cast<u16>(key)
    };
}

inline Input mouse(MouseButton button) {
    return Input{
        InputKind::MouseButton,
        static_cast<u16>(button)
    };
}

inline Input axis() {
    return Input{
        InputKind::Axis,
        static_cast<u16>(0)
    };
}

// Short text for a keybind, like "Ctrl+Z" or "M+V"
std::string keybindLabel(const Keybind& keybind);
