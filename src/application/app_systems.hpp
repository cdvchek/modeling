#pragma once

#include "core/events/event_dispatcher.hpp"
#include "core/input/input_state.hpp"
#include "core/input/action_map.hpp"

struct Systems {
    EventDispatcher events;
    InputState input;
    ActionMap actions;
};