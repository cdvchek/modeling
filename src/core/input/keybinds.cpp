#include "core/input/keybinds.hpp"

namespace {
    std::string keyName(Key key) {
        const u16 code = static_cast<u16>(key);

        if (key >= Key::A && key <= Key::Z) return std::string(1, static_cast<char>('A' + (code - static_cast<u16>(Key::A))));
        if (key >= Key::Num0 && key <= Key::Num9) return std::string(1, static_cast<char>('0' + (code - static_cast<u16>(Key::Num0))));
        if (key >= Key::F1 && key <= Key::F12) return "F" + std::to_string(code - static_cast<u16>(Key::F1) + 1);

        switch (key) {
            case Key::Escape: return "Esc";
            case Key::Enter: return "Enter";
            case Key::Tab: return "Tab";
            case Key::Backspace: return "Backspace";
            case Key::Space: return "Space";
            case Key::LeftShift: case Key::RightShift: return "Shift";
            case Key::LeftCtrl: case Key::RightCtrl: return "Ctrl";
            case Key::LeftAlt: case Key::RightAlt: return "Alt";
            case Key::Delete: return "Del";
            case Key::ArrowUp: return "Up";
            case Key::ArrowDown: return "Down";
            case Key::ArrowLeft: return "Left";
            case Key::ArrowRight: return "Right";
            default: return "?";
        }
    }

    std::string mouseName(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return "LMB";
            case MouseButton::Right: return "RMB";
            case MouseButton::Middle: return "MMB";
            case MouseButton::B4: return "Mouse4";
            case MouseButton::B5: return "Mouse5";
            default: return "Mouse";
        }
    }
}

std::string keybindLabel(const Keybind& keybind) {
    std::string label;

    for (const Input& input : keybind.inputs) {
        if (!label.empty()) label += "+";

        switch (input.kind) {
            case InputKind::Key: label += keyName(static_cast<Key>(input.code)); break;
            case InputKind::MouseButton: label += mouseName(static_cast<MouseButton>(input.code)); break;
            case InputKind::Axis: label += "Wheel"; break;
        }
    }

    return label;
}
