#include "core/console/console_input.hpp"

void updateConsoleInput(Console& console, CommandSystem& commands, const ConsoleActions& keys,
                        const ActionMap& actions, const InputState& input, u32 context) {
    const auto repeated = [&](ActionId action) { return actions.wasActionPressedOrRepeated(action, input, context); };

    if (repeated(keys.cursorLeft)) console.moveCursorLeft();
    if (repeated(keys.cursorRight)) console.moveCursorRight();
    if (repeated(keys.historyOlder)) console.viewOlderCommand();
    if (repeated(keys.historyNewer)) console.viewNewerCommand();
    if (actions.wasActionPressedThisFrame(keys.enter, input, context)) console.enterCurrentCommand(commands);
    if (repeated(keys.backspace)) console.removeFromCurrentCommandBack();
    if (repeated(keys.remove)) console.removeFromCurrentCommandForward();
}