# Console

A text console drawn over the viewport, plus a registry of named commands.

Files: `shared/core/console/` (the console, the command registry, and the keys that edit its line), [shared/ui/console_view.hpp](../ui/console_view.hpp) (its panel), and each program's own commands: Valuma's in [application_commands.cpp](../../valuma/src/application/application_commands.cpp) and [light_commands.cpp](../../valuma/src/application/commands/light_commands.cpp), Aevora's in [editor_actions.cpp](../../aevora/editor/editor_actions.cpp).

Everything but the commands is shared, so the console looks and behaves the same in every program. A program calls `updateConsoleView` and `updateConsoleInput` (with a `ConsoleActions` naming its own actions for Enter, Backspace, Delete, and the arrows) while its console is open, and `drawConsole` last when drawing. Each program's commands are in its own docs: [Valuma's](../../valuma/docs/commands.md), [Aevora's](../../aevora/docs/editor.md#commands).

Press **/** to open or close it (the `/` itself is never typed into the command). While it's open, the `Console` input context is on and only console keybinds work (see [input.md](input.md)). Typed characters arrive through `Event::Char`. Backspace, Delete, the arrow keys, and history Up/Down repeat while held, after the system repeat delay (`ActionMap::wasActionPressedOrRepeated`); Enter doesn't.

**Look** ([console_view.cpp](../ui/console_view.cpp), `drawConsole`): a panel docked at the bottom of the viewport above the status bar (45% of the height, at most 420 px), styled like the floating panel. A header shows "Console" and dim key hints. The list above the input shows **entries**, newest at the bottom: commands (dim green `>`, fading as they get older), their output (grey), and errors (red, with a `!`). Lines too long for the panel wrap onto further rows (after a space when there is one), indented under the text, without a new prompt. Output and errors longer than one line show a small arrow: click the row to collapse it to its first line plus "(+N lines)", or expand it again; the clicked row stays in view and the expanded lines appear below it. The mouse wheel scrolls the list (three lines a notch); new entries jump back to the newest. While Up/Down is recalling a command (`Console::getBrowsedIndex()`, -1 once you edit or go past the newest), that entry is highlighted like a selected list row, and the list scrolls only as far as needed to keep it in view; a thin scrollbar appears when there's more than fits. Scroll position, the rows that can be clicked, and the last frame's list rect live in `ctx.consoleView` ([console_view_state.hpp](../ui/console_view.hpp)); `updateConsoleView` (called from `checkConsoleContext`) handles the wheel and clicks.

**Collapsing rule:** closing the console calls `Console::collapseEntries()`, so every multi-line entry already there opens collapsed next time. Entries added after that (even while the console is closed, like a tool's error) stay expanded until the next close. One-line entries and commands never collapse. The input line is an outlined field with a green `>` prompt; the command scrolls sideways to keep the caret in view. The caret (`Console::getCursor()`) is a 2 px green bar that stays solid while the command or cursor changes and then blinks every 0.5 s. Sizes and colors are constants at the top of the file.

## Console

[console.hpp](../core/console/console.hpp)

Holds the line being typed, the cursor, the history of entered lines (for Up/Down), and the list of **entries** (`ConsoleEntry`: kind `Command` / `Output` / `Error`, text that may span lines, `expanded`, and for commands their `historyIndex`).

| Method | Description |
|---|---|
| `insertToCurrentCommand(char)` | Inserts a character at the cursor. |
| `removeFromCurrentCommandBack()` / `removeFromCurrentCommandForward()` | Backspace / Delete. |
| `moveCursorLeft()` / `moveCursorRight()` / `setCursor(u32)` | Cursor movement. |
| `viewOlderCommand()` / `viewNewerCommand()` | Step through history (↑ / ↓). |
| `enterCurrentCommand(CommandSystem&)` | Adds the line to history and as a command entry (if non-empty), runs it with `std::cout` redirected, adds whatever it printed as one output entry, adds an error entry for an unknown command, and clears the line. |
| `print(text)` / `printError(text)` | Adds an output or error entry (trailing line breaks dropped, empty text ignored). With `setEcho(true)` (the app turns it on) entries are also written to stdout. Tools report refusals with `printError` (bevel, extrude, inset, dissolve). |
| `getEntries()`, `toggleEntry(index)`, `collapseEntries()` | The entry list; collapse/expand one multi-line entry; collapse every multi-line entry (on close). |
| `getBrowsedIndex()` / `getCursor()` | The history entry Up/Down recalled (-1 when typing a new command) and the caret position, for drawing. |
| `getCurrentCommand()` / `getHistory()` | Read by `drawConsole`. |

## CommandSystem

[command_system.hpp](../core/console/command_system.hpp)

| Method | Description |
|---|---|
| `registerCommand(name, description, callback)` | Adds a command. `callback` is `void(const CommandArgs&)`, where `CommandArgs` is `std::vector<std::string>`. Descriptions are shown by `help`; keep them as "what it does: usage" with no trailing period and `<value>` placeholders, ideally short enough to fit on one console line (about 100 characters; longer ones wrap). |
| `execute(line)` | Splits on whitespace; the first word is the command name and the rest are arguments. Returns false for an unknown name (an empty line is fine). |
| `setGuard(allowed)` | Asked with a command's name before it runs; returning false skips it (the guard prints why) and `execute` still returns true. The app uses it to refuse modeling commands outside the Model workspace. |
| `list()` | Every command and its description, sorted by name (used by `help`). |
| `commandName(line)` | The first word of a line. |
