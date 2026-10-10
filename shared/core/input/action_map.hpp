#pragma once

#include <types>
#include <functional>
#include <string_view>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include "core/input/keybinds.hpp"
#include "core/input/input_state.hpp"
#include "core/input/context_manager.hpp"

// An action's number. Each program lists its own actions in an enum, and a value of it converts by itself.
struct ActionId {
    u16 value;

    template <typename E> requires std::is_enum_v<E>
    constexpr ActionId(E action) : value(static_cast<u16>(action)) {}
    constexpr explicit ActionId(u16 value) : value(value) {}

    // Back to the program's enum
    template <typename E> requires std::is_enum_v<E>
    constexpr E as() const { return static_cast<E>(value); }
};

// What a one-shot action does, so keys and menus can run it the same way
struct ActionHandler {
    std::string_view label;
    std::function<bool()> canRun;
    std::function<void()> run;
};

class ActionMap {
public:
    void subscribe(ActionId action, Keybind keybind, u32 input_ctx);

    bool isActionDown(ActionId action, const InputState& input, u32 input_ctx, i32* axis_value = nullptr) const;
    bool wasActionPressedThisFrame(ActionId action, const InputState& input, u32 input_ctx) const;

    // While blocked, actions bound to a mouse button or the scroll wheel never fire (the UI has the mouse)
    void setMouseBlocked(bool blocked) { m_mouseBlocked = blocked; }

    // While blocked, actions bound to keys never fire (a text field has the keyboard), except unblockable ones
    void setKeyboardBlocked(bool blocked) { m_keyboardBlocked = blocked; }
    // An action the keyboard block never stops (quitting)
    void setUnblockable(ActionId action) { m_unblockable.push_back(action.value); }

    // Contexts that own the input while they're active: only bindings in the owner's context work.
    // When several are active the earliest wins (a modal window over the console).
    void setOwnerContexts(std::vector<u32> owners) { m_owners = std::move(owners); }

    // An action the filter turns down doesn't fire from its binding, can't run from a menu, and isn't available
    // (a workspace's own set of tools); no filter allows everything
    void setFilter(std::function<bool(ActionId)> allowed) { m_filter = std::move(allowed); }
    bool isAllowed(ActionId action) const { return !m_filter || m_filter(action); }

    // Like wasActionPressedThisFrame, but a held single-key binding also fires on each OS key repeat
    bool wasActionPressedOrRepeated(ActionId action, const InputState& input, u32 input_ctx) const;

    void setHandler(ActionId action, ActionHandler handler);
    const ActionHandler* getHandler(ActionId action) const;
    bool canRun(ActionId action) const;

    // Could it run from a menu right now: canRun passes and, if it has a binding, one of its contexts is active
    bool isAvailable(ActionId action, u32 input_ctx) const;
    const Keybind* getKeybind(ActionId action) const;

    // Runs the handler of every action pressed this frame, in the order of their numbers, rechecking the context
    // after each one
    void dispatch(const InputState& input, const ContextManager& input_ctx) const;

private:
    bool usesMouse(const Keybind& keybind) const;
    bool usesKeys(const Keybind& keybind) const;
    bool isBlocked(ActionId action, const Keybind& keybind) const;

    // A key binding doesn't fire while Ctrl or Alt is held unless it includes it, so Ctrl+S never also scales.
    // Shift only counts for bindings with Ctrl or Alt, so Ctrl+S and Ctrl+Shift+S stay apart.
    bool modifiersMatch(const Keybind& keybind, const InputState& input) const;

    // While an owner context is active, only bindings in it work
    bool ownerAllows(u32 bindingContexts, u32 input_ctx) const;

    struct ActionData {
        Keybind bind;
        u32 ctx;
    };

    std::unordered_map<u16, ActionData> m_keybinds;
    std::unordered_map<u16, ActionHandler> m_handlers;
    std::vector<u16> m_unblockable;
    std::vector<u32> m_owners;
    bool m_mouseBlocked = false;
    bool m_keyboardBlocked = false;
    std::function<bool(ActionId)> m_filter;
};
