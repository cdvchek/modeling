# The editor

Files: `aevora/editor/`

[editor.hpp](../editor/editor.hpp)

Built the same way as Valuma: one `EditorContext` holds everything and is passed by reference, and `namespace Editor` is free functions.

| File | Contents |
|---|---|
| `main.cpp` | Starts up, opens the project file given as the first command-line argument (if any), runs, shuts down. |
| `editor.cpp` | `initialize` (window, OpenGL context, fonts, UI renderer), `registerEvents`, `run`, `render`, `shutdown`, the window title. |
| `editor_actions.cpp` | `registerActions`, `registerCommands`, `update` (the frame's keys and any file dialog asked for), `newProject`, `openProject`. |
| `editor_ui.cpp` | `drawInterface`: the top bar, the start screen or the workspace, the status bar, the console. |

**Frame:**

```
input.beginFrame()
Platform::pollEvents()
ui.beginFrame(makeUIInput(input), console closed)
actions.setMouseBlocked / setKeyboardBlocked from the UI
update()     // a file dialog asked for last frame, jobs.runMainTasks, actions.dispatch, console keys
render()     // clear, drawInterface, draw the UI, present
```

The editor draws through the shared OpenGL code (`OpenGLContext`, `OpenGLFont`, `OpenGLUIRenderer`) directly; it has no scene renderer yet.

**Screen:**

- **Top bar:** the workspace tabs (World, Scene, Data) once a project is open, then the project's name and the New and Open buttons on the right.
- **Middle:** with no project open, a card with New project and Open project. With one open, the current workspace, which for now only says its name.
- **Status bar:** the frame rate, and with a project open the workspace and the project's folder. An error printed while the console is closed shows here for five seconds.
- **Console:** over the middle while it's open (see [console.md](../../shared/docs/console.md)).

**File dialogs** never open in the middle of input handling or drawing. A key, button, or command sets `ctx.request`; `update` opens the dialog at the start of the next frame and then calls `input.releaseAll()`, because the dialog takes the key-ups of keys that were held when it opened. New project picks a folder, which becomes the project (named after the folder). Open project picks a `.aev` file. Both dialogs start in the folder that holds the last project, or Documents.

## Actions and keys

Aevora's own lists are in `aevora/editor/input/` ([actions.hpp](../editor/input/actions.hpp), [contexts.hpp](../editor/input/contexts.hpp), [default_keybinds.hpp](../editor/input/default_keybinds.hpp)); the mechanism is the shared one in [input.md](../../shared/docs/input.md). Contexts: `Global` (always on), `Editor` (the one mode so far), and `Console`, which owns the input while it's open. `Quit` gets past the keyboard block.

| Key | Action | Works in |
|---|---|---|
| Alt+F4 | Quit | Everywhere |
| / | Open or close the console | Everywhere |
| Enter, Backspace, Delete, Left, Right, Up, Down | The console's editing keys | Console |
| Ctrl+N | New project | Editor |
| Ctrl+O | Open project | Editor |

## Commands

| Command | What it does |
|---|---|
| `help` | Lists every command and what it does. |
| `quit` | Closes the editor. |
| `project` | Shows the open project's name and file. `project new` and `project open` open the dialogs. |
| `jobs` | Shows the job system's workers and what they're doing (see [jobs.md](jobs.md)). |
| `workspace` | Shows the current workspace. `workspace world`, `scene`, or `data` switches to it. |
