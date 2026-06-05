#pragma once

#include <types>
#include <vector>
#include <unordered_map>

#include "core/input/actions.hpp"
#include "core/input/keybinds.hpp"
#include "core/input/input_state.hpp"

class ActionMap {
public:
    void subscribe(Action action, Keybind keybind);

    bool isActionDown(Action action, const InputState& input) const;
    bool wasActionPressedThisFrame(Action action, const InputState& input) const;

private:
    std::unordered_map<Action, Keybind> m_keybinds;
};