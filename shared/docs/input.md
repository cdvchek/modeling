# Input

Turns raw key and mouse state into named actions that only fire in the right mode.

Files: `shared/core/input/`

This is the mechanism, and it knows nothing about any program's actions or modes: an action is a number (`ActionId`) and a context is a bit flag. Each program lists its own actions, contexts, and default keys: [Valuma's](../../valuma/docs/input.md), [Aevora's](../../aevora/docs/editor.md#actions-and-keys).

| Piece | File | Job |
|---|---|---|
| `InputState` | [input_state.hpp](../core/input/input_state.hpp) | Current and previous-frame key/mouse state |
| `Keybind` | [keybinds.hpp](../core/input/keybinds.hpp) | Which inputs trigger an action |
| `ContextManager` | [context_manager.hpp](../core/input/context_manager.hpp) | Which contexts (modes) are active right now |
| `ActionMap` | [action_map.hpp](../core/input/action_map.hpp) | Answers "is this action happening?" using the other three |

## InputState

Filled by the program's event subscribers. `beginFrame()` runs at the top of every frame: it copies current state into "previous", zeroes mouse delta and scroll, and clears the frame's button presses and releases. Button events also carry the click's position, which is applied first: Windows can deliver the move to a spot after the click there.

| Method | Description |
|---|---|
| `isKeyDown(key)` / `wasKeyDown(key)` | Held this frame / held last frame |
| `wasKeyPressedThisFrame(key)` / `wasKeyReleasedThisFrame(key)` | Edge detection: down now but not last frame, and vice versa |
| `isMouseDown`, `wasMouseDown` | Held this frame / last frame, for mouse buttons |
| `wasMousePressedThisFrame` / `wasMouseReleasedThisFrame` | A press or release happened this frame, recorded as it arrived, so a click that goes down and up within one frame reports both (and `ActionMap` counts it as a press) |
| `getMouseX/Y()` | Cursor position in client pixels (origin top-left) |
| `getMouseDeltaX/Y()` | Movement since the start of this frame, every move added up |
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

Each program keeps its defaults in a `namespace DefaultKeybinds` of its own.

## Input contexts

A context is a bit flag from a program's own `InputContext` enum; several can be on at once. A binding names the contexts it works in and only fires while one of them is active.

### ContextManager

Shared, and set up by each program: `ContextManager(alwaysOn, modes)` takes the contexts that are always active and a list of **modes**, contexts of which only one is active at a time, in order of priority; the first is active to begin with. A default-constructed one has neither, so its contexts are plain flags. Valuma's comes from `makeInputContexts()` in `contexts.hpp`: `Global` always on, and the four selection contexts as modes (vertex mode first).

| Method | Description |
|---|---|
| `setContext(u32)` | Replaces all contexts (always-on ones are kept). Used to enter and leave modal tools, e.g. `setContext(InputContext_Grab)` turns off selection mode until `setContext(getModeContext())`. |
| `setModeContext(u32)` | Switches mode (Valuma's selection mode). Keeps exactly one mode bit (asked for several, the earliest in the list wins: Vertex > Edge > Face > Object) and remembers it. Ignored if it holds no mode. |
| `addContext` / `removeContext` / `toggleContext` | Change the other bits. Always-on and mode bits are ignored here. |
| `isActive(u32 mask)` | True if **any** bit in `mask` is on. |
| `getContext()` | All active bits. Pass this to `ActionMap`. |
| `getModeContext()` | The remembered mode, even while a modal tool is running. |

## ActionMap

Actions are passed as `ActionId`, a number that any enum value converts to by itself, so call sites just write `Action::GrabSelection`. `id.as<Action>()` goes back to the enum.

| Method | Description |
|---|---|
| `subscribe(action, Keybind, u32 contexts)` | Binds an action to a key combo, valid in any of `contexts`. One binding per action. |
| `isActionDown(action, input, context, i32* axis = nullptr)` | True while every input in the combo is held. For `axis()` bindings, writes the scroll delta to `axis`. |
| `wasActionPressedThisFrame(action, input, context)` | True on the frame the combo becomes complete (all held, at least one newly pressed). |
| `setKeyboardBlocked(bool)` | While set, actions whose binding includes a key never fire, except unblockable ones. Set every frame from `ctx.ui.wantsKeyboard()`, so typing in a text field doesn't trigger shortcuts. |
| `setUnblockable(action)` | An action the keyboard block never stops. Valuma sets `Quit`. |
| `setOwnerContexts(owners)` | Contexts that own the input while they're active: only bindings that list the owner can fire. When several are active the earliest wins. Valuma sets `Modal`, then `Console`. |
| `wasActionPressedOrRepeated(action, input, context)` | Like `wasActionPressedThisFrame`, and also true on each OS repeat of a single-key binding that's held. The console's editing keys use it. |
| `setMouseBlocked(bool)` | While set, actions whose binding includes a mouse button or the scroll wheel never fire. Set every frame from `ctx.ui.wantsMouse()` so clicks and scrolling over UI don't reach the viewport. |
| `setFilter(allowed)` / `isAllowed(action)` | A function that says whether an action exists right now (the app passes `actionAllowed` for the current workspace). An action it turns down doesn't fire from its binding, `canRun` is false, and it isn't available to menus; no filter allows everything. |
| `dispatch` and chords | When a binding fires together with a longer one that contains all its keys (F inside M+F), only the longer one runs, so one press is one action. Handlers still run in the order of their numbers (enum order), each checked again after the ones before it. |

Both return false if none of the action's contexts are active, or if an owner context is active and the action doesn't list it: in Valuma a modal window (`Modal`), then the console (`Console`) (`ownerAllows`).

**Modifiers:** a binding made only of keys doesn't fire while Ctrl or Alt is held unless it includes it, so Ctrl+S saves without also starting scale (S). Shift only counts for bindings that have Ctrl or Alt, so Ctrl+S and Ctrl+Shift+S stay apart while Shift+key bindings could still be added. Either side's modifier counts as held. Bindings with a mouse button or the wheel are unaffected (Shift/Ctrl/Alt + click are separate actions checked by the selection code).

### Handlers (the action registry)

One-shot actions also have a handler, so the keyboard and the radial menu run them the same way:

```cpp
struct ActionHandler {
    std::string_view label;        // short name for menus ("Grab", "Extrude")
    std::function<bool()> canRun;  // can it run right now? (empty = always)
    std::function<void()> run;
};
```

| Method | Description |
|---|---|
| `setHandler(action, ActionHandler)` | Registers (or replaces) the handler. |
| `getHandler(action)` | The handler, or `nullptr`. |
| `canRun(action)` | True if it has a handler whose `canRun` passes. Doesn't look at keys or contexts. |
| `isAvailable(action, context)` | `canRun`, and if the action has a binding, one of its contexts is active. The radial menu dims items where this is false. |
| `getKeybind(action)` | The binding, or `nullptr` for actions with none (shown as key hints in the radial menu). |
| `dispatch(input, contextManager)` | Runs the handler of every action pressed this frame whose `canRun` passes, in the order of their numbers (`Action` enum order). The context is read again before each action, so an action that starts a tool stops the ones after it that only work outside the tool. |

`keybindLabel(keybind)` ([keybinds.cpp](../core/input/keybinds.cpp)) turns a binding into short text such as `Ctrl+Z`, `M+V`, `Del`, or `Mouse4`.
