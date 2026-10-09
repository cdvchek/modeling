# Console

A text console drawn over the viewport, plus a registry of named commands.

Files: `src/core/console/`, commands in [application_commands.cpp](../../src/application/application_commands.cpp) and [light_commands.cpp](../../src/application/commands/light_commands.cpp)

Press **/** to open or close it (the `/` itself is never typed into the command). While it's open, the `Console` input context is on and only console keybinds work (see [input.md](input.md)). Typed characters arrive through `Event::Char`. Backspace, Delete, the arrow keys, and history Up/Down repeat while held, after the system repeat delay (`ActionMap::wasActionPressedOrRepeated`); Enter doesn't.

**Look** ([console_view.cpp](../../src/application/ui/console_view.cpp), `drawConsole`): a panel docked at the bottom of the viewport above the status bar (45% of the height, at most 420 px), styled like the floating panel. A header shows "Console" and dim key hints. The list above the input shows **entries**, newest at the bottom: commands (dim green `>`, fading as they get older), their output (grey), and errors (red, with a `!`). Lines too long for the panel wrap onto further rows (after a space when there is one), indented under the text, without a new prompt. Output and errors longer than one line show a small arrow: click the row to collapse it to its first line plus "(+N lines)", or expand it again; the clicked row stays in view and the expanded lines appear below it. The mouse wheel scrolls the list (three lines a notch); new entries jump back to the newest. While Up/Down is recalling a command (`Console::getBrowsedIndex()`, -1 once you edit or go past the newest), that entry is highlighted like a selected list row, and the list scrolls only as far as needed to keep it in view; a thin scrollbar appears when there's more than fits. Scroll position, the rows that can be clicked, and the last frame's list rect live in `ctx.consoleView` ([console_view_state.hpp](../../src/application/ui/console_view_state.hpp)); `updateConsoleView` (called from `checkConsoleContext`) handles the wheel and clicks.

**Collapsing rule:** closing the console calls `Console::collapseEntries()`, so every multi-line entry already there opens collapsed next time. Entries added after that (even while the console is closed, like a tool's error) stay expanded until the next close. One-line entries and commands never collapse. The input line is an outlined field with a green `>` prompt; the command scrolls sideways to keep the caret in view. The caret (`Console::getCursor()`) is a 2 px green bar that stays solid while the command or cursor changes and then blinks every 0.5 s. Sizes and colors are constants at the top of the file.

## Console

[console.hpp](../../src/core/console/console.hpp)

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

[command_system.hpp](../../src/core/console/command_system.hpp)

| Method | Description |
|---|---|
| `registerCommand(name, description, callback)` | Adds a command. `callback` is `void(const CommandArgs&)`, where `CommandArgs` is `std::vector<std::string>`. Descriptions are shown by `help`; keep them as "what it does: usage" with no trailing period and `<value>` placeholders, ideally short enough to fit on one console line (about 100 characters; longer ones wrap). |
| `execute(line)` | Splits on whitespace; the first word is the command name and the rest are arguments. Returns false for an unknown name (an empty line is fine). |
| `list()` | Every command and its description, sorted by name (used by `help`). |
| `commandName(line)` | The first word of a line. |

## Commands

| Command | Arguments | Description |
|---|---|---|
| `help` | — | Lists every command with its description. |
| `save` | none / `<path>` | Saves the project to its file (the Save As dialog the first time), or to the path. `.vlm` is added when there's no extension, and a relative path goes in `Documents\Valuma Studio`. See [project.md](project.md). |
| `open` | none / `<path>` | Opens a project (the Open dialog without a path). Asks to save unsaved changes first. |
| `new` | — | Starts a new project with the default scene. Asks to save unsaved changes first. |
| `origin` | `geometry` / `bottom` / `world` / `rotation` / `selection` | Moves an origin while the mesh stays put: to the middle of the bounding box, the middle of its bottom, the world's 0, 0, 0, back in line with the world's axes, or the average of the selected vertices. Acts on the selected origin's object, else the active object (`selection` always the active object). Undoable; prints where the origin ended up. |
| `import` | none / `<path>` | Adds a `.vlmobj` asset as a new object (the file dialog without a path). `.vlmobj` is added when there's no extension; a relative path starts in the export folder. See [vlmobj.md](vlmobj.md#in-the-app). |
| `fileinfo` | none / `<path>` | Lists a project file's header and sections (type, version, offset, size, checksum, object names and counts). Without a path, the current project's file. |
| `debug` | `on` / `off` / none (toggle) | Shows the half-edge debug overlay. |
| `validate` | — | Runs `MeshData::validate()` on the active object. Prints counts if OK, or the first broken invariant. |
| `merge` | `center` (default) / `first` / `last` | Vertex mode, exactly 2 selected, connected by an edge: collapses them into one vertex at the chosen position. Undoable. |
| `dissolve` | — | Edge mode with 1 edge: collapses it to its midpoint. Face mode with 1 face: collapses it to its center. Undoable. |
| `light` | see [below](#light-command) | Lists, adds, removes, and edits scene lights and the ambient light. Undoable. |
| `stats` | none / `on` / `off` | Shows or hides the stats readout: CPU input and render time, GPU time, the last click's pick time, draw calls, triangles, lines, points, uniform uploads, mesh rebuilds (Uploads) and in-place updates (Patches) with their bytes, and the scene's objects, faces, and vertices. With vsync on, CPU render time includes waiting for the screen, so turn vsync off to measure. |
| `material` | see [below](#material-command) | Lists, adds, removes, edits, and assigns materials. Undoable. |
| `shading` | none / `flat` / `smooth` / `auto [<degrees>]` / `mark hard\|smooth\|clear` | No argument prints the active object's shading and how many edges are marked. A mode applies to the selected objects in object mode, otherwise the active object (`auto` with an angle, 0 to 180, sets that too). `mark` marks the selected edges in edge mode. Undoable. |
| `reference` | see [below](#reference-command) | Lists, adds, removes, and edits reference images. Undoable. |
| `exposure` | none / `<stops>` | Prints or sets the exposure, −5 to 5 (0 is normal; each +1 doubles how bright lit surfaces look). A viewport setting saved with the project, not undoable. |
| `headlight` | none / `on` / `off` / `color <r> <g> <b>` / `strength <s>` | Prints or changes the camera headlight (values 0 to 1). A viewport setting, not undoable. |
| `backface` | `tint` / `tint <r> <g> <b>` | Prints or sets the color back faces are multiplied by (each 0 to 1; `1 1 1` turns the tint off). A display setting stored on the renderer, so not undoable. |
| `ui` | `panel` / `materials` / `checker` | Shows or hides the floating panel (shown by default), switches between material view and clay view, or shows or hides the UV checker grid. |
| `vsync` | `on` / `off` / none (toggle) | Turns vertical sync on or off and prints the new state. Off lets the frame rate (status bar FPS) go past the monitor's refresh rate. Not saved; starts on each launch. |
| `object` | see [below](#object-command) | Lists, adds, removes, edits, and switches the active object. |

Anything a command writes to `std::cout` shows up in the console under it (and in stdout). An unknown command adds an error entry. A command can also add entries itself with `print`/`printError` (the project commands do); with echo on, those go to the real stdout rather than into the command's captured output, so they appear once.

## Light command

A light's `<id>` is its slot index, shown by `light list` and printed by `light add`. It stays the same while the light exists; a removed light's id may be reused by the next `light add`.

| Form | Description |
|---|---|
| `light list` | Prints every light and the ambient light. |
| `light ambient` / `ambient color <r> <g> <b>` / `ambient strength <s>` | Prints or sets the ambient light (values 0 to 1). |
| `light add <point \| directional \| spot> [name]` | Adds a light with default settings and prints its id. Name defaults to the next "Light N". |
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

## Material command

A material's `<id>` is its slot index, shown by `material list`; Default is always 0.

| Form | Description |
|---|---|
| `material list` | Prints every material: name, base color, roughness, metallic, emissive (when it glows), alpha mode with opacity and cutoff, single- or double-sided, and how many objects and faces use it. |
| `material add [name]` | Adds a material with default values (named "Material", numbered if taken). |
| `material <id>` | Prints one material. |
| `material <id> remove` | Removes it; its objects go back to Default. Default can't be removed. |
| `material <id> assign [<object id> ...]` | Gives it to those objects; without ids, to the selected faces in face mode, otherwise to the selected objects (object mode) or the active object. |
| `material <id> select` | Face mode: selects the active object's faces that show it (their own, or the object's when they have none). |
| `material clear` | Face mode: the selected faces stop having their own material and use the object's again. |
| `material <id> name <n>` | Renames it (one word). |
| `material <id> color <r> <g> <b>` / `emissive <r> <g> <b>` | Base and emissive color, each 0 to 1 (sRGB). |
| `material <id> roughness` / `metallic` / `opacity` / `cutoff <0 to 1>` | |
| `material <id> glow <strength>` | Emissive strength, 0 or more (0 doesn't glow). |
| `material <id> mode <opaque \| cutout \| blend>` | Alpha mode. |
| `material <id> sides <single \| double>` | Whether back faces are drawn in material view (and in the game). |

Every change is one undo step; bad input prints the usage for that property and changes nothing.

## Reference command

An image's `<id>` is its slot index, shown by `reference list`.

| Form | Description |
|---|---|
| `reference list` | Prints every reference image: name, file and size in pixels, shown or hidden, locked, opacity, depth, position, rotation (degrees), and size. |
| `reference add` / `reference add <file.png>` | Adds images from the file dialog, or one file. `.png` is added when there's no extension; a relative path starts in `Documents\Valuma Studio`. Placed facing the view at the camera's target, selected. Not while a tool runs. |
| `reference <id>` | Prints one image. |
| `reference <id> remove` | Removes it. |
| `reference <id> show` / `hide` | Shows or hides it. |
| `reference <id> lock` / `unlock` | A locked image ignores clicks in the viewport. |
| `reference <id> name <n>` | Renames it (one word). |
| `reference <id> opacity <0 to 1>` | How see-through it is, on top of the picture's own transparency. |
| `reference <id> depth <behind \| scene \| front>` | Under everything, among the meshes, or over everything. |
| `reference <id> position <x> <y> <z>` / `rotation <x> <y> <z>` | Rotation in degrees. |
| `reference <id> size <s>` | Its height in units; the width follows the picture. |

Every change is one undo step; bad input prints the usage for that property and changes nothing.

## Object command

An object's `<id>` is its slot index, shown by `object list`.

| Form | Description |
|---|---|
| `object list` | Prints every object: name, `(editing)` for the active one, vertex and face counts, position, rotation (degrees), scale (relative to the parent), and its parent if it has one. |
| `object add <preset> [name]` | Adds a preset (`cube`, `plane`, `grid`, `circle`, `cylinder`, `cone`, `uvsphere`, `icosphere`, `torus`), 1.5 units further along X than the last, and makes it the active object. The name defaults to the preset's name, numbered if taken ("Cube 2"). |
| `object <id>` | Prints one object. |
| `object <id> edit` | Makes it the active object. |
| `object <id> remove` | Removes it. If it was active, the first remaining object becomes active. |
| `object <id> name <n>` | Renames it (one word). |
| `object <id> parent <id \| none>` | Gives it a parent, or none, keeping it where it is in the world. Refused if the parent is the object or one of its children. |
| `object <id> position <x> <y> <z>` | Moves it. |
| `object <id> rotation <x> <y> <z>` | Rotation in degrees. |
| `object <id> scale <x> <y> <z>` | Scale; no component may be zero. |

Add, remove, and edits are undoable.

## Adding a command

Register it in `Application::registerCommands`:

```cpp
ctx.systems.commands.registerCommand(
    "name",
    "What it does: name <value>",
    [&ctx](const CommandArgs& args) {
        ctx.history.begin(ctx.scene);
        // ... edit, then commit or cancel
    }
);
```

Then add it to the table above and to [features.md](../features.md).
