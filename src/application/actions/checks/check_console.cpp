#include "application/actions/checks/action_checks.hpp"
#include "application/ui/console_view.hpp"

void checkConsoleContext(AppContext& ctx) {
    updateConsoleView(ctx);

    // Editing keys repeat while held, after the OS repeat delay; Enter doesn't
    if (ctx.systems.actions.wasActionPressedOrRepeated(
        Action::ConsoleCursorLeft,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.moveCursorLeft();
    }

    if (ctx.systems.actions.wasActionPressedOrRepeated(
        Action::ConsoleCursorRight,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.moveCursorRight();
    }

    if (ctx.systems.actions.wasActionPressedOrRepeated(
        Action::ConsoleHistoryOlder,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.viewOlderCommand();
    }

    if (ctx.systems.actions.wasActionPressedOrRepeated(
        Action::ConsoleHistoryNewer,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.viewNewerCommand();
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::EnterCommand,
        ctx.systems.input, 
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.enterCurrentCommand(ctx.systems.commands);
    }

    if (ctx.systems.actions.wasActionPressedOrRepeated(
        Action::ConsoleBackspace,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.removeFromCurrentCommandBack();
    }

    if (ctx.systems.actions.wasActionPressedOrRepeated(
        Action::ConsoleDelete,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext())) {
        ctx.systems.console.removeFromCurrentCommandForward();
    }
}