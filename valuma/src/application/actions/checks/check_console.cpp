#include "application/actions/checks/action_checks.hpp"
#include "core/console/console_input.hpp"

void checkConsoleContext(AppContext& ctx) {
    updateConsoleView(ctx.systems.console, ctx.consoleView, ctx.systems.input);

    const ConsoleActions keys {
        Action::EnterCommand, Action::ConsoleBackspace, Action::ConsoleDelete,
        Action::ConsoleCursorLeft, Action::ConsoleCursorRight, Action::ConsoleHistoryOlder, Action::ConsoleHistoryNewer
    };
    updateConsoleInput(ctx.systems.console, ctx.systems.commands, keys, ctx.systems.actions, ctx.systems.input, ctx.systems.input_ctx.getContext());
}