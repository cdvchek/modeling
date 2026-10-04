# Input

Turns raw key and mouse state into named actions that only fire in the right mode.

Files: `src/core/input/`

There are five pieces:

| Piece | File | Job |
|---|---|---|
| `InputState` | [input_state.hpp](../../src/core/input/input_state.hpp) | Current and previous-frame key/mouse state |
| `Action` | [actions.hpp](../../src/core/input/actions.hpp) | Enum of every named intent |
| `Keybind` / `DefaultKeybinds` | [keybinds.hpp](../../src/core/input/keybinds.hpp) | Which inputs trigger each action |
| `InputContext` / `ContextManager` | [contexts.hpp](../../src/core/input/contexts.hpp), [context_manager.hpp](../../src/core/input/context_manager.hpp) | Which modes are active right now |
| `ActionMap` | [action_map.hpp](../../src/core/input/action_map.hpp) | Answers "is this action happening?" using the other three |

## InputState

Filled by event subscribers in `application_events.cpp`. `beginFrame()` runs at the top of every frame: it copies current state into "previous" and zeroes mouse delta and scroll.

| Method | Description |
|---|---|
| `isKeyDown(key)` / `wasKeyDown(key)` | Held this frame / held last frame |
| `wasKeyPressedThisFrame(key)` / `wasKeyReleasedThisFrame(key)` | Edge detection: down now but not last frame, and vice versa |
| `isMouseDown`, `wasMouseDown`, `wasMousePressedThisFrame`, `wasMouseReleasedThisFrame` | Same, for mouse buttons |
| `getMouseX/Y()` | Cursor position in client pixels (origin top-left) |
| `getMouseDeltaX/Y()` | Movement since the start of this frame |
| `getScroll()` | Wheel delta this frame (0 if none) |
| `onKey`, `onMouseButton`, `onMouseMove`, `onScroll` | Called by event subscribers to record input |

## Keybinds

A `Keybind` is a list of `Input`s that must **all** be held. Build inputs with the helpers `key(Key::G)`, `mouse(MouseButton::Left)`, and `axis()` (the mouse wheel).

```cpp
inline Keybind Undo { { key(Key::LeftCtrl), key(Key::Z) } };
```

Defaults live in `namespace DefaultKeybinds`.

## Input contexts

`InputContext` is a bit flag enum. Several can be on at once.

| Context | Meaning |
|---|---|
| `Global` | Always on |
| `Console` | Console is open. **While on, only actions that list `Console` can fire.** |
| `Debug` | Half-edge debug overlay is visible |
| `SelectionVertex` / `SelectionEdge` / `SelectionFace` | Current selection mode. Exactly one is on, unless a modal tool replaced it. |
| `Grab` / `Scale` / `Rotate` / `Bevel` | A modal tool is running |
| `XAxis` / `YAxis` / `ZAxis` | Axis lock during grab/scale/rotate |

### ContextManager

| Method | Description |
|---|---|
| `setContext(u32)` | Replaces all contexts (Global is always kept). Used to enter and leave modal tools, e.g. `setContext(InputContext_Grab)` turns off selection mode until `setContext(getSelectionContext())`. |
| `setSelectionContext(u32)` | Switches selection mode. Keeps exactly one selection bit (priority Vertex > Edge > Face) and remembers it. |
| `addContext` / `removeContext` / `toggleContext` | Change non-selection bits. Global and selection bits are ignored here. |
| `isActive(u32 mask)` | True if **any** bit in `mask` is on. |
| `getContext()` | All active bits. Pass this to `ActionMap`. |
| `getSelectionContext()` | The remembered selection mode, even while a modal tool is running. |

## ActionMap

| Method | Description |
|---|---|
| `subscribe(Action, Keybind, u32 contexts)` | Binds an action to a key combo, valid in any of `contexts`. One binding per action. |
| `isActionDown(action, input, context, i32* axis = nullptr)` | True while every input in the combo is held. For `axis()` bindings, writes the scroll delta to `axis`. |
| `wasActionPressedThisFrame(action, input, context)` | True on the frame the combo becomes complete (all held, at least one newly pressed). |

Both return false if none of the action's contexts are active, or if the console is open and the action isn't a console action.

All bindings are registered in [application_actions.cpp](../../src/application/application_actions.cpp).

## Default keybinds

"Selection" means vertex, edge, or face mode is active. "+" means hold together.

| Action | Keys | Contexts |
|---|---|---|
| Quit | Alt + F4 | Global |
| ToggleConsole | Tab | Global, Console |
| ViewportOrbit | Right mouse drag | Selection |
| ViewportPan | Middle mouse drag | Selection |
| ViewportZoom | Mouse wheel | Selection |
| VertexMode | M + V | Selection |
| EdgeMode | M + E | Selection |
| FaceMode | M + F | Selection |
| Select | Left click | Selection |
| ToggleSelection | Shift + left click | Selection |
| SelectLoop | Ctrl + left click | Edge, Face |
| SelectRing | Alt + left click | Edge |
| Undo | Ctrl + Z | Selection |
| Redo | Ctrl + Y | Selection |
| GrabSelection | G | Selection |
| ScaleSelection | S | Selection |
| RotateSelection | R | Selection |
| BevelSelection | B | Selection |
| ExtrudeSelection | E | Face |
| InsetSelection | I | Face |
| DeleteSelection | Delete | Selection |
| FillFaceLoop | F | Edge |
| ConnectVertices | C | Vertex |
| ConfirmGrab / ConfirmScale / RotateConfirm / ConfirmBevel | Left click | that tool |
| CancelGrab / CancelScale / RotateCancel / CancelBevel | Right click | that tool |
| XAxis / YAxis / ZAxis | X / Y / Z (toggles) | Grab, Scale, Rotate |
| EnterCommand | Enter | Console |
| ConsoleBackspace / ConsoleDelete | Backspace / Delete | Console |
| ConsoleCursorLeft / Right | ← / → | Console |
| ConsoleHistoryOlder / Newer | ↑ / ↓ | Console |

## Adding an action

1. Add a value to `enum class Action` (before `Count`).
2. Add a `Keybind` in `DefaultKeybinds`.
3. `actions.subscribe(...)` it in `registerDefaultActions` with the contexts where it should work.
4. Check it in the matching `check*Context` function (see [application.md](application.md)).
5. Add it to the table above and to [features.md](../features.md).

Combos that share keys can both fire: Shift+click also satisfies `Select`. The selection code checks the more specific action first.
