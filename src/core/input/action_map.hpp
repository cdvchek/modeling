#pragma once

#include <types>
#include <vector>
#include <unordered_map>

#include "core/input/actions.hpp"
#include "core/input/keybinds.hpp"
#include "core/input/input_state.hpp"
#include "core/input/context_manager.hpp"

class ActionMap {
public:
    void subscribe(Action action, Keybind keybind, u32 input_ctx);

    bool isActionDown(Action action, const InputState& input, u32 input_ctx, i32* axis_value = nullptr) const;
    bool wasActionPressedThisFrame(Action action, const InputState& input, u32 input_ctx) const;

private:
    struct ActionData {
        Keybind bind;
        u32 ctx;
    };

    std::unordered_map<Action, ActionData> m_keybinds;
};