# Console

A text console drawn over the viewport, plus a registry of named commands.

Files: `src/core/console/`, commands in [application_commands.cpp](../../src/application/application_commands.cpp)

Press **Tab** to open or close it. While it's open, the `Console` input context is on and only console keybinds work (see [input.md](input.md)). Typed characters arrive through `Event::Char`.

## Console

[console.hpp](../../src/core/console/console.hpp)

Holds the line being typed, the cursor, and the history of entered lines.

| Method | Description |
|---|---|
| `insertToCurrentCommand(char)` | Inserts a character at the cursor. |
| `removeFromCurrentCommandBack()` / `removeFromCurrentCommandForward()` | Backspace / Delete. |
| `moveCursorLeft()` / `moveCursorRight()` / `setCursor(u32)` | Cursor movement. |
| `viewOlderCommand()` / `viewNewerCommand()` | Step through history (↑ / ↓). |
| `enterCurrentCommand(CommandSystem&)` | Runs the line, adds it to history if non-empty, and clears it. |
| `getCurrentCommand()` / `getHistory()` | Read by `renderFrame` to draw the console. |

## CommandSystem

[command_system.hpp](../../src/core/console/command_system.hpp)

| Method | Description |
|---|---|
| `registerCommand(name, description, callback)` | Adds a command. `callback` is `void(const CommandArgs&)`, where `CommandArgs` is `std::vector<std::string>`. |
| `execute(line)` | Splits on whitespace; the first word is the command name and the rest are arguments. Unknown names are ignored. |

Descriptions are stored but not shown anywhere yet.

## Commands

| Command | Arguments | Description |
|---|---|---|
| `debug` | `on` / `off` / none (toggle) | Shows the half-edge debug overlay. |
| `validate` | — | Runs `MeshData::validate()` on object 0. Prints counts if OK, or the first broken invariant. |
| `merge` | `center` (default) / `first` / `last` | Vertex mode, exactly 2 selected, connected by an edge: collapses them into one vertex at the chosen position. Undoable. |
| `dissolve` | — | Edge mode with 1 edge: collapses it to its midpoint. Face mode with 1 face: collapses it to its center. Undoable. |
| `light` | `ambient` / `ambient color <r> <g> <b>` / `ambient strength <s>` | Prints or sets the ambient light. Values must be 0 to 1; bad input prints usage. Undoable. Only `ambient` exists so far; per-light targets come later. |
| `headlight` | none / `on` / `off` / `color <r> <g> <b>` / `strength <s>` | Prints or changes the camera headlight (values 0 to 1). A viewport setting, not undoable. |
| `backface` | `tint` / `tint <r> <g> <b>` | Prints or sets the color back faces are multiplied by (each 0 to 1; `1 1 1` turns the tint off). A display setting stored on the renderer, so not undoable. |
| `test` | any | Prints its arguments to stdout. |

Output goes to stdout/stderr, not to the console overlay.

## Adding a command

Register it in `Application::registerCommands`:

```cpp
ctx.systems.commands.registerCommand(
    "name",
    "What it does.",
    [&ctx](const CommandArgs& args) {
        ctx.history.begin(ctx.scene);
        // ... edit, then commit or cancel
    }
);
```

Then add it to the table above and to [features.md](../features.md).
