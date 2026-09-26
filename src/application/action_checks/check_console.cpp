#include "application/action_checks/action_checks.hpp"

void checkConsoleContext(AppContext& ctx) {
    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::ConsoleCursorLeft,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.moveCursorLeft();
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::ConsoleCursorRight,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.moveCursorRight();
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::ConsoleHistoryOlder,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.viewOlderCommand();
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::ConsoleHistoryNewer,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.viewNewerCommand();
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::EnterCommand,
        ctx.systems.input, 
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.enterCurrentCommand();
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::ConsoleBackspace,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.removeFromCurrentCommandBack();
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::ConsoleDelete,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.removeFromCurrentCommandForward();
    }
}