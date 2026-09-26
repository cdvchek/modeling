#pragma once

#include "core/events/event_dispatcher.hpp"
#include "core/input/input_state.hpp"
#include "core/input/action_map.hpp"
#include "core/input/context_manager.hpp"
#include "core/console/console.hpp"

struct Systems {
    EventDispatcher events;
    InputState input;
    ActionMap actions;
    ContextManager input_ctx;
    Console console;
};