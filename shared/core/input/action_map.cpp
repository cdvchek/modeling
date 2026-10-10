#include "core/input/action_map.hpp"

#include <algorithm>
#include <utility>

void ActionMap::subscribe(ActionId action, Keybind keybind, u32 input_ctx) {
    m_keybinds.emplace(action.value, ActionData{keybind, input_ctx});
}

void ActionMap::setHandler(ActionId action, ActionHandler handler) {
    m_handlers[action.value] = std::move(handler);
}

const ActionHandler* ActionMap::getHandler(ActionId action) const {
    auto it = m_handlers.find(action.value);
    return it == m_handlers.end() ? nullptr : &it->second;
}

bool ActionMap::canRun(ActionId action) const {
    if (!isAllowed(action)) return false;
    const ActionHandler* handler = getHandler(action);
    return handler && handler->run && (!handler->canRun || handler->canRun());
}

bool ActionMap::isAvailable(ActionId action, u32 input_ctx) const {
    auto it = m_keybinds.find(action.value);
    if (it != m_keybinds.end() && !(it->second.ctx & input_ctx)) return false;
    return canRun(action);
}

const Keybind* ActionMap::getKeybind(ActionId action) const {
    auto it = m_keybinds.find(action.value);
    return it == m_keybinds.end() ? nullptr : &it->second.bind;
}

void ActionMap::dispatch(const InputState& input, const ContextManager& input_ctx) const {
    // A chord that fires (M+F) hides the bindings made of some of its own keys (F), so one press is one action
    std::vector<ActionId> pressed;
    for (const auto& [value, handler] : m_handlers) {
        const ActionId action(value);
        if (wasActionPressedThisFrame(action, input, input_ctx.getContext())) pressed.push_back(action);
    }
    std::sort(pressed.begin(), pressed.end(), [](ActionId a, ActionId b) { return a.value < b.value; });

    const auto partOf = [this](ActionId smaller, ActionId larger) {
        const Keybind& small = m_keybinds.at(smaller.value).bind;
        const Keybind& large = m_keybinds.at(larger.value).bind;
        if (small.inputs.size() >= large.inputs.size()) return false;
        for (const Input& a : small.inputs) {
            bool found = false;
            for (const Input& b : large.inputs) found = found || (a.kind == b.kind && a.code == b.code);
            if (!found) return false;
        }
        return true;
    };

    for (ActionId action : pressed) {
        bool hidden = false;
        for (ActionId other : pressed) hidden = hidden || partOf(action, other);
        if (hidden) continue;

        // Earlier handlers can change the context, so each is checked again
        if (wasActionPressedThisFrame(action, input, input_ctx.getContext()) && canRun(action)) m_handlers.at(action.value).run();
    }
}

bool ActionMap::usesKeys(const Keybind& keybind) const {
    for (const Input& input : keybind.inputs) {
        if (input.kind == InputKind::Key) return true;
    }
    return false;
}

bool ActionMap::isBlocked(ActionId action, const Keybind& keybind) const {
    if (!isAllowed(action)) return true;
    if (m_mouseBlocked && usesMouse(keybind)) return true;
    if (!m_keyboardBlocked || !usesKeys(keybind)) return false;
    return std::find(m_unblockable.begin(), m_unblockable.end(), action.value) == m_unblockable.end();
}

bool ActionMap::ownerAllows(u32 bindingContexts, u32 input_ctx) const {
    for (u32 owner : m_owners) {
        if (input_ctx & owner) return bindingContexts & owner;
    }
    return true;
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

bool ActionMap::wasActionPressedOrRepeated(ActionId action, const InputState& input, u32 input_ctx) const {
    if (wasActionPressedThisFrame(action, input, input_ctx)) return true;

    auto it = m_keybinds.find(action.value);
    if (it == m_keybinds.end()) return false;

    const ActionData& data = it->second;
    const std::vector<Input>& inputs = data.bind.inputs;

    // Repeats only make sense for one key held on its own
    if (inputs.size() != 1 || inputs[0].kind != InputKind::Key) return false;
    if (!(input_ctx & data.ctx)) return false;
    if (!ownerAllows(data.ctx, input_ctx)) return false;
    if (isBlocked(action, data.bind) || !modifiersMatch(data.bind, input)) return false;

    return input.wasKeyPressedOrRepeated(inputs[0].code);
}

bool ActionMap::usesMouse(const Keybind& keybind) const {
    for (const Input& input : keybind.inputs) {
        if (input.kind == InputKind::MouseButton || input.kind == InputKind::Axis) return true;
    }
    return false;
}

bool ActionMap::isActionDown(ActionId action, const InputState& input, u32 input_ctx, i32* axis_value) const {
    auto it = m_keybinds.find(action.value);

    if (it == m_keybinds.end()) {
        return false;
    }

    const ActionData& data = it->second;
    const Keybind& keybind = data.bind;
    const u32 key_context = data.ctx;

    bool contextMatch = input_ctx & key_context;
    if (!contextMatch) return false;

    if (!ownerAllows(key_context, input_ctx)) return false;

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

bool ActionMap::wasActionPressedThisFrame(ActionId action, const InputState& input, u32 input_ctx) const {
    auto it = m_keybinds.find(action.value);

    if (it == m_keybinds.end()) {
        return false;
    }

    const ActionData& data = it->second;
    const Keybind& keybind = data.bind;
    const u32 key_context = data.ctx;

    bool contextMatch = input_ctx & key_context;
    if (!contextMatch) return false;

    if (!ownerAllows(key_context, input_ctx)) return false;

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
                // A click that went down and up within the frame still counts as a press
                if (input.wasMousePressedThisFrame(bindingInput.code)) {
                    anyPressedThisFrame = true;
                } else if (!input.isMouseDown(bindingInput.code)) {
                    return false;
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