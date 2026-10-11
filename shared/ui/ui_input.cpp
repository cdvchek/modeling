#include "ui/ui_input.hpp"

#include <chrono>
#include "core/input/input_state.hpp"

UIInput makeUIInput(const InputState& input) {
    const u16 buttons[UIInput::BUTTON_COUNT] = {
        static_cast<u16>(MouseButton::Left), static_cast<u16>(MouseButton::Right), static_cast<u16>(MouseButton::Middle)
    };

    UIInput result;
    result.mouse = Vec2(static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()));
    result.mouseDelta = Vec2(static_cast<f32>(input.getMouseDeltaX()), static_cast<f32>(input.getMouseDeltaY()));
    result.scroll = input.getScroll();

    for (u32 i = 0; i < UIInput::BUTTON_COUNT; ++i) {
        result.down[i] = input.isMouseDown(buttons[i]);
        result.pressed[i] = input.wasMousePressedThisFrame(buttons[i]);
        result.released[i] = input.wasMouseReleasedThisFrame(buttons[i]);
    }

    auto held = [&](Key a, Key b) { return input.isKeyDown(static_cast<u16>(a)) || input.isKeyDown(static_cast<u16>(b)); };
    const bool ctrl = held(Key::LeftCtrl, Key::RightCtrl);
    result.shift = held(Key::LeftShift, Key::RightShift);
    result.text = input.getTypedText();

    // Key presses include the OS's repeats, so held keys repeat in text fields too
    for (u16 code : input.getKeyPresses()) {
        switch (static_cast<Key>(code)) {
            case Key::ArrowLeft: result.keys.push_back(UIKey::Left); break;
            case Key::ArrowRight: result.keys.push_back(UIKey::Right); break;
            case Key::Home: result.keys.push_back(UIKey::Home); break;
            case Key::End: result.keys.push_back(UIKey::End); break;
            case Key::Backspace: result.keys.push_back(UIKey::Backspace); break;
            case Key::Delete: result.keys.push_back(UIKey::Delete); break;
            case Key::Enter: result.keys.push_back(UIKey::Enter); break;
            case Key::Escape: result.keys.push_back(UIKey::Escape); break;
            case Key::A: if (ctrl) result.keys.push_back(UIKey::SelectAll); break;
            case Key::C: if (ctrl) result.keys.push_back(UIKey::Copy); break;
            case Key::X: if (ctrl) result.keys.push_back(UIKey::Cut); break;
            case Key::V: if (ctrl) result.keys.push_back(UIKey::Paste); break;
            default: break;
        }
    }

    result.time = std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return result;
}
