#include "core/input/action_map.hpp"

#include <utility>

void ActionMap::subscribe(Action action, Keybind keybind, u32 input_ctx) {
    m_keybinds.emplace(action, ActionData{keybind, input_ctx});
}

void ActionMap::setHandler(Action action, ActionHandler handler) {
    m_handlers[action] = std::move(handler);
}

const ActionHandler* ActionMap::getHandler(Action action) const {
    auto it = m_handlers.find(action);
    return it == m_handlers.end() ? nullptr : &it->second;
}

bool ActionMap::canRun(Action action) const {
    const ActionHandler* handler = getHandler(action);
    return handler && handler->run && (!handler->canRun || handler->canRun());
}

bool ActionMap::isAvailable(Action action, u32 input_ctx) const {
    auto it = m_keybinds.find(action);
    if (it != m_keybinds.end() && !(it->second.ctx & input_ctx)) return false;
    return canRun(action);
}

const Keybind* ActionMap::getKeybind(Action action) const {
    auto it = m_keybinds.find(action);
    return it == m_keybinds.end() ? nullptr : &it->second.bind;
}

void ActionMap::dispatch(const InputState& input, const ContextManager& input_ctx) const {
    for (u32 i = 0; i < static_cast<u32>(Action::Count); i++) {
        const Action action = static_cast<Action>(i);
        if (!m_handlers.contains(action)) continue;

        if (wasActionPressedThisFrame(action, input, input_ctx.getContext()) && canRun(action)) {
            m_handlers.at(action).run();
        }
    }
}

bool ActionMap::usesKeys(const Keybind& keybind) const {
    for (const Input& input : keybind.inputs) {
        if (input.kind == InputKind::Key) return true;
    }
    return false;
}

bool ActionMap::isBlocked(Action action, const Keybind& keybind) const {
    if (m_mouseBlocked && usesMouse(keybind)) return true;
    return m_keyboardBlocked && action != Action::Quit && usesKeys(keybind);
}

bool ActionMap::modifiersMatch(const Keybind& keybind, const InputState& input) const {
    if (usesMouse(keybind)) return true;

    auto bound = [&](Key left, Key right) {
        for (const Input& binding : keybind.inputs) {
            if (binding.kind == InputKind::Key && (binding.code == static_cast<u16>(left) || binding.code == static_cast<u16>(right))) return true;
        }
        return false;
    };
    auto held = [&](Key left, Key right) {
        return input.isKeyDown(static_cast<u16>(left)) || input.isKeyDown(static_cast<u16>(right));
    };

    const bool ctrl = bound(Key::LeftCtrl, Key::RightCtrl);
    const bool alt = bound(Key::LeftAlt, Key::RightAlt);
    const bool shift = bound(Key::LeftShift, Key::RightShift);

    if (!ctrl && held(Key::LeftCtrl, Key::RightCtrl)) return false;
    if (!alt && held(Key::LeftAlt, Key::RightAlt)) return false;
    if ((ctrl || alt) && !shift && held(Key::LeftShift, Key::RightShift)) return false;
    return true;
}

bool ActionMap::wasActionPressedOrRepeated(Action action, const InputState& input, u32 input_ctx) const {
    if (wasActionPressedThisFrame(action, input, input_ctx)) return true;

    auto it = m_keybinds.find(action);
    if (it == m_keybinds.end()) return false;

    const ActionData& data = it->second;
    const std::vector<Input>& inputs = data.bind.inputs;

    // Repeats only make sense for one key held on its own
    if (inputs.size() != 1 || inputs[0].kind != InputKind::Key) return false;
    if (!(input_ctx & data.ctx)) return false;
    if (input_ctx & InputContext_Console && !(data.ctx & InputContext_Console)) return false;
    if (isBlocked(action, data.bind) || !modifiersMatch(data.bind, input)) return false;

    return input.wasKeyPressedOrRepeated(inputs[0].code);
}

bool ActionMap::usesMouse(const Keybind& keybind) const {
    for (const Input& input : keybind.inputs) {
        if (input.kind == InputKind::MouseButton || input.kind == InputKind::Axis) return true;
    }
    return false;
}

bool ActionMap::isActionDown(Action action, const InputState& input, u32 input_ctx, i32* axis_value) const {
    auto it = m_keybinds.find(action);

    if (it == m_keybinds.end()) {
        return false;
    }

    const ActionData& data = it->second;
    const Keybind& keybind = data.bind;
    const u32 key_context = data.ctx;

    bool contextMatch = input_ctx & key_context;
    if (!contextMatch) return false;

    // Only console bindings work while the console is open.
    if (input_ctx & InputContext_Console && !(key_context & InputContext_Console)) return false;

    if (isBlocked(action, keybind) || !modifiersMatch(keybind, input)) return false;

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

bool ActionMap::wasActionPressedThisFrame(Action action, const InputState& input, u32 input_ctx) const {
    auto it = m_keybinds.find(action);

    if (it == m_keybinds.end()) {
        return false;
    }

    const ActionData& data = it->second;
    const Keybind& keybind = data.bind;
    const u32 key_context = data.ctx;

    bool contextMatch = input_ctx & key_context;
    if (!contextMatch) return false;

    if (input_ctx & InputContext_Console && !(key_context & InputContext_Console)) return false;

    if (isBlocked(action, keybind) || !modifiersMatch(keybind, input)) return false;

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