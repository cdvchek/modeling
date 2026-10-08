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
| `getKeyPresses()` / `wasKeyPressedOrRepeated(key)` | Every key press this frame in order, including the OS's repeats while a key is held (they start after the system repeat delay). Used for text fields and console editing keys. |
| `getTypedText()` | Printable characters typed this frame (from `Event::Char`; control characters such as Backspace, Enter, and Ctrl+letters are left out) |
| `onKey`, `onChar`, `onMouseButton`, `onMouseMove`, `onScroll` | Called by event subscribers to record input |
| `releaseAll()` | Forgets every held key and button without reporting releases. Called after a modal dialog, which takes the key-ups for keys held when it opened (like Ctrl). |

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
| `SelectionVertex` / `SelectionEdge` / `SelectionFace` / `SelectionObject` | Current selection mode. Exactly one is on, unless a modal tool replaced it. `InputContext_EditModes` is the first three; `InputContext_AnySelection` is all four. |
| `Grab` / `Scale` / `Rotate` / `Bevel` / `Inset` | A modal tool is running |
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
| `setKeyboardBlocked(bool)` | While set, actions whose binding includes a key never fire, except `Quit`. Set every frame from `ctx.ui.wantsKeyboard()`, so typing in a text field doesn't trigger shortcuts. |
| `wasActionPressedOrRepeated(action, input, context)` | Like `wasActionPressedThisFrame`, and also true on each OS repeat of a single-key binding that's held. The console's editing keys use it. |
| `setMouseBlocked(bool)` | While set, actions whose binding includes a mouse button or the scroll wheel never fire. Set every frame from `ctx.ui.wantsMouse()` so clicks and scrolling over UI don't reach the viewport. |

Both return false if none of the action's contexts are active, or if the console is open and the action isn't a console action.

**Modifiers:** a binding made only of keys doesn't fire while Ctrl or Alt is held unless it includes it, so Ctrl+S saves without also starting scale (S). Shift only counts for bindings that have Ctrl or Alt, so Ctrl+S and Ctrl+Shift+S stay apart while Shift+key bindings could still be added. Either side's modifier counts as held. Bindings with a mouse button or the wheel are unaffected (Shift/Ctrl/Alt + click are separate actions checked by the selection code).

All bindings are registered in [application_actions.cpp](../../src/application/application_actions.cpp).

### Handlers (the action registry)

One-shot actions also have a handler, so the keyboard and (later) the radial menu run them the same way:

```cpp
struct ActionHandler {
    std::string_view label;        // short name for menus ("Grab", "Extrude")
    std::function<bool()> canRun;  // can it run right now? (empty = always)
    std::function<void()> run;
};
```

| Method | Description |
|---|---|
| `setHandler(Action, ActionHandler)` | Registers (or replaces) the handler. |
| `getHandler(action)` | The handler, or `nullptr`. |
| `canRun(action)` | True if it has a handler whose `canRun` passes. Doesn't look at keys or contexts. |
| `isAvailable(action, context)` | `canRun`, and if the action has a binding, one of its contexts is active. The radial menu dims items where this is false. |
| `getKeybind(action)` | The binding, or `nullptr` for actions with none (shown as key hints in the radial menu). |
| `dispatch(input, contextManager)` | Runs the handler of every action pressed this frame whose `canRun` passes, in `Action` enum order. The context is read again before each action, so an action that starts a tool stops the ones after it that only work outside the tool. |

Some actions have a handler but no binding (merge, dissolve, Free, light type, light on/off, panel, headlight, debug view); they only run from the radial menu.

`keybindLabel(keybind)` ([keybinds.cpp](../../src/core/input/keybinds.cpp)) turns a binding into short text such as `Ctrl+Z`, `M+V`, `Del`, or `Mouse4`.

Handlers are registered in `registerDefaultActions`; their lambdas capture the `AppContext` and call the named functions in [editing_actions.hpp](../../src/application/editing_actions.hpp). Continuous and held actions (orbit, pan, zoom, picking, console editing) and a running tool's confirm/cancel have no handler; their `check*Context` code reads them directly.

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

1. Add a value to `enum class Action` (before `Count`).
2. Add a `Keybind` in `DefaultKeybinds`.
3. `actions.subscribe(...)` it in `registerDefaultActions` with the contexts where it should work.
4. If it's a one-shot action, write a function for it (and a `can*` check if it isn't always available) and `setHandler` it in `registerDefaultActions`. Otherwise read it in the matching `check*Context` function (see [application.md](application.md)).
5. Add it to the table above and to [features.md](../features.md).

Combos that share keys can both fire: Shift+click also satisfies `Select`. The selection code checks the more specific action first.
