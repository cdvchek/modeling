#pragma once

#include <types>
#include <functional>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "core/input/actions.hpp"
#include "core/input/keybinds.hpp"
#include "core/input/input_state.hpp"
#include "core/input/context_manager.hpp"

// What a one-shot action does, so keys and menus can run it the same way
struct ActionHandler {
    std::string_view label;
    std::function<bool()> canRun;
    std::function<void()> run;
};

class ActionMap {
public:
    void subscribe(Action action, Keybind keybind, u32 input_ctx);

    bool isActionDown(Action action, const InputState& input, u32 input_ctx, i32* axis_value = nullptr) const;
    bool wasActionPressedThisFrame(Action action, const InputState& input, u32 input_ctx) const;

    // While blocked, actions bound to a mouse button or the scroll wheel never fire (the UI has the mouse)
    void setMouseBlocked(bool blocked) { m_mouseBlocked = blocked; }

    // While blocked, actions bound to keys never fire (a text field has the keyboard); Quit still works
    void setKeyboardBlocked(bool blocked) { m_keyboardBlocked = blocked; }

    // Like wasActionPressedThisFrame, but a held single-key binding also fires on each OS key repeat
    bool wasActionPressedOrRepeated(Action action, const InputState& input, u32 input_ctx) const;

    void setHandler(Action action, ActionHandler handler);
    const ActionHandler* getHandler(Action action) const;
    bool canRun(Action action) const;

    // Could it run from a menu right now: canRun passes and, if it has a binding, one of its contexts is active
    bool isAvailable(Action action, u32 input_ctx) const;
    const Keybind* getKeybind(Action action) const;

    // Runs the handler of every action pressed this frame, in enum order, rechecking the context after each one
    void dispatch(const InputState& input, const ContextManager& input_ctx) const;

private:
    bool usesMouse(const Keybind& keybind) const;
    bool usesKeys(const Keybind& keybind) const;
    bool isBlocked(Action action, const Keybind& keybind) const;

    // A key binding doesn't fire while Ctrl or Alt is held unless it includes it, so Ctrl+S never also scales.
    // Shift only counts for bindings with Ctrl or Alt, so Ctrl+S and Ctrl+Shift+S stay apart.
    bool modifiersMatch(const Keybind& keybind, const InputState& input) const;

    // While a modal window or the console is open it owns the input: only bindings in its context work
    // (a modal window wins over the console)
    bool ownerAllows(u32 bindingContexts, u32 input_ctx) const;

    struct ActionData {
        Keybind bind;
        u32 ctx;
    };

    std::unordered_map<Action, ActionData> m_keybinds;
    std::unordered_map<Action, ActionHandler> m_handlers;
    bool m_mouseBlocked = false;
    bool m_keyboardBlocked = false;
};