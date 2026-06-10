#include "core/input/action_map.hpp"

void ActionMap::subscribe(Action action, Keybind keybind) {
    m_keybinds.emplace(action, keybind);
}

bool ActionMap::isActionDown(Action action, const InputState& input, i32* axis_value) const {
    auto it = m_keybinds.find(action);

    if (it == m_keybinds.end()) {
        return false;
    }

    const Keybind& keybind = it->second;

    for (const Input& bindingInput : keybind.inputs) {
        switch (bindingInput.kind) {
            case InputKind::Key:
                if (!input.isKeyDown(bindingInput.code)) {
                    return false;
                }
                break;
            case InputKind::MouseButton:
                if (!input.isMouseDown(bindingInput.code)) {
                    return false;
                }
                break;
            case InputKind::Axis:
                if (input.getScroll() == 0) return false;
                if (axis_value) *axis_value = input.getScroll();
                break;
        }
    }

    return true;
}

bool ActionMap::wasActionPressedThisFrame(Action action, const InputState& input) const {
    auto it = m_keybinds.find(action);

    if (it == m_keybinds.end()) {
        return false;
    }

    const Keybind& keybind = it->second;

    bool anyPressedThisFrame = false;

    for (const Input& bindingInput : keybind.inputs) {
        switch (bindingInput.kind) {
            case InputKind::Key:
                if (!input.isKeyDown(bindingInput.code)) {
                    return false;
                }

                if (input.wasKeyPressedThisFrame(bindingInput.code)) {
                    anyPressedThisFrame = true;
                }
                break;

            case InputKind::MouseButton:
                if (!input.isMouseDown(bindingInput.code)) {
                    return false;
                }

                if (input.wasMousePressedThisFrame(bindingInput.code)) {
                    anyPressedThisFrame = true;
                }
                break;

            case InputKind::Axis:
                if (input.getScroll() == 0) return false;
                anyPressedThisFrame = true;
                break;
        }
    }

    return anyPressedThisFrame;
}