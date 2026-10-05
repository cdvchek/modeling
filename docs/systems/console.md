# Console

A text console drawn over the viewport, plus a registry of named commands.

Files: `src/core/console/`, commands in [application_commands.cpp](../../src/application/application_commands.cpp) and [light_commands.cpp](../../src/application/light_commands.cpp)

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
| `light` | see [below](#light-command) | Lists, adds, removes, and edits scene lights and the ambient light. Undoable. |
| `headlight` | none / `on` / `off` / `color <r> <g> <b>` / `strength <s>` | Prints or changes the camera headlight (values 0 to 1). A viewport setting, not undoable. |
| `backface` | `tint` / `tint <r> <g> <b>` | Prints or sets the color back faces are multiplied by (each 0 to 1; `1 1 1` turns the tint off). A display setting stored on the renderer, so not undoable. |
| `ui` | `panel` | Shows or hides the floating panel (shown by default). |
| `vsync` | `on` / `off` / none (toggle) | Turns vertical sync on or off and prints the new state. Off lets the frame rate (status bar FPS) go past the monitor's refresh rate. Not saved; starts on each launch. |
| `test` | any | Prints its arguments to stdout. |

Output goes to stdout/stderr, not to the console overlay.

## Light command

A light's `<id>` is its slot index, shown by `light list` and printed by `light add`. It stays the same while the light exists; a removed light's id may be reused by the next `light add`.

| Form | Description |
|---|---|
| `light list` | Prints every light and the ambient light. |
| `light ambient` / `ambient color <r> <g> <b>` / `ambient strength <s>` | Prints or sets the ambient light (values 0 to 1). |
| `light add <point \| directional \| spot> [name]` | Adds a light with default settings and prints its id. Name defaults to the type. |
| `light <id>` | Prints one light. |
| `light <id> remove` | Removes it. |
| `light <id> on` / `off` | Enables or disables it without removing it. |
| `light <id> name <n>` | Renames it (one word). |
| `light <id> type <point \| directional \| spot>` | Changes its type; other settings are kept. |
| `light <id> color <r> <g> <b>` | Color, each 0 to 1. |
| `light <id> intensity <i>` | Brightness multiplier, 0 or more. |
| `light <id> position <x> <y> <z>` | Point and spot. |
| `light <id> direction <x> <y> <z>` | Directional and spot; direction the light travels. Normalized; can't be all zero. |
| `light <id> range <r>` | Point and spot; distance where the light fades to zero. More than 0. |
| `light <id> cone <inner> <outer>` | Spot; half-angles in degrees, full brightness inside `inner`, fading to zero at `outer`. `0 <= inner <= outer < 90`. |

Every change goes through `history.begin`/`commit`, so Ctrl+Z undoes it. Bad input prints the usage for that property and changes nothing.

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
