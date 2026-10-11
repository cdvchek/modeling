#pragma once

#include "core/input/keybinds.hpp"

namespace DefaultKeybinds {
    inline Keybind Quit { { key(Key::LeftAlt), key(Key::F4) } };
    inline Keybind ToggleConsole { { key(Key::Forwardslash) } };
    inline Keybind EnterCommand { { key(Key::Enter) } };
    inline Keybind ConsoleBackspace { { key(Key::Backspace) } };
    inline Keybind ConsoleDelete { { key(Key::Delete) } };
    inline Keybind ConsoleCursorLeft { { key(Key::ArrowLeft) } };
    inline Keybind ConsoleCursorRight { { key(Key::ArrowRight) } };
    inline Keybind ConsoleHistoryOlder { { key(Key::ArrowUp) } };
    inline Keybind ConsoleHistoryNewer { { key(Key::ArrowDown) } };
    inline Keybind NewProject { { key(Key::LeftCtrl), key(Key::N) } };
    inline Keybind OpenProject { { key(Key::LeftCtrl), key(Key::O) } };
}
