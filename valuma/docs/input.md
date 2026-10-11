# Input

Valuma's actions, input contexts, and default keys. The mechanism they plug into (`InputState`, `Keybind`, `ContextManager`, `ActionMap`) is shared: see the [shared input page](../../shared/docs/input.md).

Files: `valuma/src/input/`

| Piece | File | Job |
|---|---|---|
| `Action` | [actions.hpp](../src/input/actions.hpp) | Enum of every named intent |
| `DefaultKeybinds` | [default_keybinds.hpp](../src/input/default_keybinds.hpp) | The keys for each action |
| `InputContext` | [contexts.hpp](../src/input/contexts.hpp) | Valuma's modes, and `makeInputContexts()`, which sets up the `ContextManager` |

## Input contexts

`InputContext` is Valuma's bit flag enum. Several can be on at once.

| Context | Meaning |
|---|---|
| `Global` | Always on |
| `Console` | Console is open. **While on, only actions that list `Console` can fire.** |
| `Modal` | A modal window is open (Export window, prompt). **While on, only actions that list `Modal` can fire**, even over the console: Enter (`ModalConfirm`) and Escape (`ModalCancel`). |
| `Debug` | Half-edge debug overlay is visible |
| `SelectionVertex` / `SelectionEdge` / `SelectionFace` / `SelectionObject` | Current selection mode. Exactly one is on, unless a modal tool replaced it. `InputContext_EditModes` is the first three; `InputContext_AnySelection` is all four. |
| `Grab` / `Scale` / `Rotate` / `Bevel` / `Inset` | A modal tool is running |
| `XAxis` / `YAxis` / `ZAxis` | Axis lock during grab/scale/rotate |

`makeInputContexts()` gives the `ContextManager` `Global` as its always-on context and the four selection contexts as its modes, vertex mode first. Selection mode is read and switched with `getModeContext()` and `setModeContext()`.

## Registering

All bindings are registered in [application_actions.cpp](../src/application/application_actions.cpp), which also sets the owner contexts and the unblockable action.

Handlers are registered in `registerDefaultActions`; their lambdas capture the `AppContext` and call the named functions in [editing_actions.hpp](../src/application/actions/editing_actions.hpp). Continuous and held actions (orbit, pan, zoom, picking, console editing) and a running tool's confirm/cancel have no handler; their `check*Context` code reads them directly.

Some actions have a handler but no binding (merge, dissolve, Free, light type, light on/off, panel, headlight, debug view, origins on/off, and the origin commands); they only run from the radial menu.

## Default keybinds

"Selection" means any selection mode (vertex, edge, face, or object) is active; "Edit" means vertex, edge, or face. "+" means hold together.

| Action | Keys | Contexts |
|---|---|---|
| Quit | Alt + F4 | Global |
| ToggleConsole | / | Global, Console |
| ViewportOrbit | Right mouse drag | Selection |
| ViewportPan | Middle mouse drag | Selection |
| ViewportZoom | Mouse wheel | Selection |
| VertexMode | M + V | Selection |
| EdgeMode | M + E | Selection |
| FaceMode | M + F | Selection |
| ObjectMode | M + O | Selection |
| ToggleObjectMode | Tab (object mode ↔ last edit mode) | Selection |
| Select | Left click | Selection |
| ToggleSelection | Shift + left click | Selection |
| SelectLoop | Ctrl + left click | Edge, Face |
| SelectRing | Alt + left click | Edge |
| Undo | Ctrl + Z | Selection |
| Redo | Ctrl + Y | Selection |
| ExportAssets | Ctrl + E (the Export window) | Selection |
| ParentToActive / ClearParents | Ctrl + P / Alt + P | Object |
| ImportAssets | Ctrl + I | Selection |
| ModalConfirm / ModalCancel | Enter / Escape | Modal |
| SaveProject | Ctrl + S | Selection |
| SaveProjectAs | Ctrl + Shift + S | Selection |
| OpenProject | Ctrl + O | Selection |
| NewProject | Ctrl + N | Selection |
| GrabSelection | G | Selection |
| ScaleSelection | S | Selection |
| RotateSelection | R | Selection |
| BevelSelection | B | Edit |
| ExtrudeSelection | E | Face |
| InsetSelection | I | Face |
| DeleteSelection | Delete | Selection |
| FillFaceLoop | F | Edge |
| ConnectVertices | C | Vertex |
| ConfirmGrab / ConfirmScale / RotateConfirm / ConfirmBevel / ConfirmInset | Left click | that tool |
| CancelGrab / CancelScale / RotateCancel / CancelBevel / CancelInset | Right click | that tool |
| XAxis / YAxis / ZAxis | X / Y / Z (toggles) | Grab, Scale, Rotate |
| RadialMenu | Mouse4 (thumb side button, held) | Global |
| EnterCommand | Enter | Console |
| ConsoleBackspace / ConsoleDelete | Backspace / Delete | Console |
| ConsoleCursorLeft / Right | ← / → | Console |
| ConsoleHistoryOlder / Newer | ↑ / ↓ | Console |

## Adding an action

1. Add a value to `enum class Action` in `valuma/src/input/actions.hpp` (before `Count`).
2. Add a `Keybind` in `DefaultKeybinds` (`valuma/src/input/default_keybinds.hpp`).
3. `actions.subscribe(...)` it in `registerDefaultActions` with the contexts where it should work.
4. If it's a one-shot action, write a function for it (and a `can*` check if it isn't always available) and `setHandler` it in `registerDefaultActions`. Otherwise read it in the matching `check*Context` function (see [application.md](application.md)).
5. Add it to the table above and to [features.md](features.md).

Combos that share keys can both fire: Shift+click also satisfies `Select`. The selection code checks the more specific action first.
