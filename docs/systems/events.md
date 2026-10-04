# Events

A small typed publish/subscribe system. The platform layer triggers events; the application subscribes to them. Neither side knows about the other.

Files: `src/core/events/`

## Event types

[events.hpp](../../src/core/events/events.hpp)

Each event is a small struct in the `Event` namespace. The `EVENT_TYPE` macro gives it a static `type` (an `EventType` enum value) and `name`.

| Event | Fields |
|---|---|
| `KeyDown` | `u16 key`, `bool repeat` |
| `KeyUp` | `u16 key` |
| `Char` | `char character` (typed text, used by the console) |
| `MouseMove` | `i32 x, y` (client coordinates) |
| `MouseButtonDown` / `MouseButtonUp` | `u16 button`, `i32 x, y` |
| `MouseWheel` | `i32 delta` |
| `WindowResize` | `u32 width, height` |
| `Quit` | — |

## EventDispatcher

[event_dispatcher.hpp](../../src/core/events/event_dispatcher.hpp), [event_dispatcher.inl](../../src/core/events/event_dispatcher.inl)

| Method | Description |
|---|---|
| `template<E> void subscribe(std::function<bool(const E&)>)` | Adds a callback for event type `E`. |
| `template<E> void trigger(const E& event)` | Calls every callback for `E` in subscription order. If a callback returns `true`, the event is consumed and later callbacks are skipped. |

Callbacks are stored type-erased in an array indexed by `EventType`. Events must be ≤ 32 bytes (`static_assert`).

## Adding an event

1. Add a value to `EventType` (before `Count`).
2. Add a struct in `namespace Event` that starts with `EVENT_TYPE(YourEvent);`.
3. Trigger it where it happens (usually a `WindowCallback` handler) and subscribe in `Application::registerInputEvents`.

## Who subscribes

All subscriptions are in [application_events.cpp](../../src/application/application_events.cpp):

- Key, mouse, and wheel events update `InputState`.
- `Char` inserts text into the console when it's open.
- `WindowResize` resizes the renderer and draws a frame immediately, because Windows blocks the main loop during a resize drag.
- `Quit` stops the main loop.
