#pragma once

#include "core/events/event_dispatcher.hpp"
#include "core/input/input_state.hpp"
#include "core/input/action_map.hpp"
#include "core/input/context_manager.hpp"
#include "core/console/console.hpp"
#include "core/console/command_system.hpp"
#include "input/actions.hpp"
#include "input/contexts.hpp"
#include "input/default_keybinds.hpp"

struct Systems {
    EventDispatcher events;
    InputState input;
    ActionMap actions;
    ContextManager input_ctx = makeInputContexts();
    Console console;
    CommandSystem commands;
};