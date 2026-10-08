#pragma once

#include <types>
#include <string>
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

// Short text for a keybind, like "Ctrl+Z" or "M+V"
std::string keybindLabel(const Keybind& keybind);

namespace DefaultKeybinds {   
    inline Keybind Quit {
        {
            key(Key::LeftAlt),
            key(Key::F4)
        }
    };

    inline Keybind ToggleConsole {
        {
            key(Key::Forwardslash)
        }
    };

    inline Keybind EnterCommand {
        {
            key(Key::Enter)
        }
    };

    inline Keybind ConsoleBackspace {
        {
            key(Key::Backspace)
        }
    };

    inline Keybind ConsoleDelete {
        {
            key(Key::Delete)
        }
    };

    inline Keybind ConsoleCursorLeft {
        {
            key(Key::ArrowLeft)
        }
    };

    inline Keybind ConsoleCursorRight {
        {
            key(Key::ArrowRight)
        }
    };

    inline Keybind ConsoleHistoryOlder {
        {
            key(Key::ArrowUp)
        }
    };

    inline Keybind ConsoleHistoryNewer {
        {
            key(Key::ArrowDown)
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

    inline Keybind VertexMode {
        {
            key(Key::M),
            key(Key::V)
        }
    };

    inline Keybind EdgeMode {
        {
            key(Key::M),
            key(Key::E)
        }
    };

    inline Keybind FaceMode {
        {
            key(Key::M),
            key(Key::F)
        }
    };
    
    inline Keybind Select {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind ToggleSelection {
        {
            mouse(MouseButton::Left),
            key(Key::LeftShift)
        }
    };

    inline Keybind SelectLoop {
        {
            mouse(MouseButton::Left),
            key(Key::LeftCtrl)
        }
    };

    inline Keybind SelectRing {
        {
            mouse(MouseButton::Left),
            key(Key::LeftAlt)
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

    inline Keybind InsetSelection {
        {
            key(Key::I)
        }
    };
    
    inline Keybind DeleteSelection {
        {
            key(Key::Delete)
        }
    };

    inline Keybind FillFaceLoop {
        {
            key(Key::F)
        }
    };

    inline Keybind ConnectVertices {
        {
            key(Key::C)
        }
    };

    inline Keybind XAxis {
        {
            key(Key::X)
        }
    };

    inline Keybind YAxis {
        {
            key(Key::Y)
        }
    };

    inline Keybind ZAxis {
        {
            key(Key::Z)
        }
    };

    inline Keybind RotateSelection {
        {
            key(Key::R)
        }
    };

    inline Keybind RotateConfirm {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind RotateCancel {
        {
            mouse(MouseButton::Right)
        }
    };

    inline Keybind BevelSelection {
        {
            key(Key::B)
        }
    };

    inline Keybind ConfirmBevel {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind CancelBevel {
        {
            mouse(MouseButton::Right)
        }
    };

    inline Keybind ObjectMode {
        {
            key(Key::M),
            key(Key::O)
        }
    };

    // Switches between object mode and the last edit mode
    inline Keybind ToggleObjectMode {
        {
            key(Key::Tab)
        }
    };

    inline Keybind ConfirmInset {
        {
            mouse(MouseButton::Left)
        }
    };

    inline Keybind CancelInset {
        {
            mouse(MouseButton::Right)
        }
    };

    inline Keybind SaveProject {
        {
            key(Key::LeftCtrl),
            key(Key::S)
        }
    };

    inline Keybind SaveProjectAs {
        {
            key(Key::LeftCtrl),
            key(Key::LeftShift),
            key(Key::S)
        }
    };

    inline Keybind OpenProject {
        {
            key(Key::LeftCtrl),
            key(Key::O)
        }
    };

    inline Keybind NewProject {
        {
            key(Key::LeftCtrl),
            key(Key::N)
        }
    };

    // The thumb side button (back)
    inline Keybind RadialMenu {
        {
            mouse(MouseButton::B4)
        }
    };
}