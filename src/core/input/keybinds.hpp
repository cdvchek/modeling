#pragma once

#include <types>
#include <vector>
#include "platform/keys/keys.hpp"

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

namespace DefaultKeybinds {   
    inline Keybind Quit {
        {
            key(Key::LeftAlt),
            key(Key::F4)
        }
    };
    
    inline Keybind ViewportOrbit {
        {
            mouse(MouseButton::Right)
        }
    };
    
    inline Keybind ViewportPan {
        {
            mouse(MouseButton::Middle)
        }
    };

    inline Keybind ViewportZoom {
        {
            axis()
        }
    };
    
    inline Keybind Select {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind AddSelection {
        {
            mouse(MouseButton::Left),
            key(Key::LeftShift)
        }
    };

    inline Keybind RemoveSelection {
        {
            mouse(MouseButton::Left),
            key(Key::LeftCtrl)
        }
    };

    inline Keybind GrabSelection {
        {
            key(Key::G)
        }
    };

    inline Keybind ConfirmGrab {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind CancelGrab {
        {
            mouse(MouseButton::Right)
        }
    };

    inline Keybind ScaleSelection {
        {
            key(Key::S)
        }
    };

    inline Keybind ConfirmScale {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind CancelScale {
        {
            mouse(MouseButton::Right)
        }
    };

    inline Keybind ExtrudeSelection {
        {
            key(Key::E)
        }
    };

    inline Keybind ConfirmExtrude {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind CancelExtrude {
        {
            mouse(MouseButton::Right)
        }
    };
    
    inline Keybind DeleteSelection {
        {
            key(Key::Backspace)
        }
    };
    
    inline Keybind Duplicate {
        {
            key(Key::D)
        }
    };
    
    inline Keybind Disconnect {
        {
            key(Key::X)
        }
    };
    
    inline Keybind Save {
        {
            key(Key::LeftCtrl),
            key(Key::S)
        }
    };
    
    inline Keybind Undo {
        {
            key(Key::LeftCtrl),
            key(Key::Z)
        }
    };
    
    inline Keybind Redo {
        {
            key(Key::LeftCtrl),
            key(Key::Y)
        }
    };
}