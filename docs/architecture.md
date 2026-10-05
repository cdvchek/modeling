# Architecture

A from-scratch C++20 modeling app on Win32 and OpenGL 3.3. No windowing, UI, or math libraries; GLAD is the only third-party code.

## Layers

```
          main.cpp
             │
       ┌─────▼──────┐
       │ application│  startup, main loop, tools (action_checks/), console commands
       └─────┬──────┘
   ┌─────────┼──────────┬──────────────┬───────────┐
┌──▼───┐ ┌───▼───┐  ┌───▼──┐   ┌─────▼────┐ ┌────▼─────┐
│ core │ │ scene │  │  ui  │   │ renderer │ │ platform │
└──────┘ └───────┘  └──────┘   └──────────┘ └──────────┘
 events   objects   draw list   IRenderer    Win32 window
 input    mesh      rects,      OpenGL impl  message pump
 console  selection text, clips shaders      GL context
 font     camera                GPU meshes   key mapping
 math     history
```

| Directory | Role | Depends on |
|---|---|---|
| `src/core/` | Engine building blocks with no app knowledge: math, containers (`DynamicArray`), events, input, console, bitmap fonts, frame timing | — |
| `src/scene/` | Everything being edited: objects, lights, half-edge meshes, selection, picking, camera, history | core (and `renderer/opengl` for `OpenGLMesh`, see below) |
| `src/ui/` | 2D UI draw list in pixel coordinates (shapes, text, clipping) and the immediate-mode widget system (`UIContext`). No OpenGL. | core (math, fonts) |
| `src/renderer/` | Backend-neutral `IRenderer` interface plus the OpenGL implementation | core, scene (mesh handles, `Scene` for the debug overlay), ui (draws a `UIDrawList`) |
| `src/platform/` | Win32 window, message pump, key translation, OpenGL context creation | core (events, keys) |
| `src/application/` | Wires everything together and owns all modeling behavior triggered by input | everything |

**Known layering leak:** `Object` (in `scene/objects/object_collection.hpp`) stores an `OpenGLMesh` directly, so the scene depends on the OpenGL backend. A backend-neutral handle would remove that.

**Platform split:** platform-specific code lives in files ending in `_win32.cpp`. The OpenGL renderer is split into `renderer/opengl/opengl_renderer_common.cpp` (portable GL drawing) and `platform/renderer/opengl_renderer_win32.cpp` (WGL context setup only). Shaders live in `renderer/opengl/shaders/` and have no platform code.

## Build targets

Defined in [CMakeLists.txt](../CMakeLists.txt):

| Target | Contents |
|---|---|
| `modeling_core` (static lib) | Math, fonts, lights, the UI draw list, and all `MeshData` code. No OpenGL or Win32, so it can be tested on its own. |
| `modeling` (exe → `bin/modeling.exe`) | Everything else plus `glad.c`, linked with `opengl32` and `dwmapi`. |
| `tests` (exe) | Every `tests/*.cpp`, linked against `modeling_core`. |

New `.cpp` files must be added to `CORE_SRC` or `APP_SRC` by hand. Tests are picked up by glob.

## The shared context

All state lives in one `AppContext` ([app_context.hpp](../src/application/app_context.hpp)), created in `main()` and passed by reference everywhere:

```cpp
struct AppContext {
    Systems systems;          // events, input, actions, input_ctx, console, commands
    std::unique_ptr<IRenderer> renderer;
    DebugRenderer debug_renderer;
    std::vector<std::unique_ptr<Window>> windows;   // only windows[0] is used
    Scene scene;              // camera, objects, lights, selection
    BevelTool bevel;          // state of an in-progress bevel
    History history;          // undo/redo
    ViewportSettings viewport; // headlight and other view-only settings
    FontLibrary fonts;        // embedded bitmap fonts
    UIDrawList uiDrawList;    // 2D UI, rebuilt every frame
    FrameTimer frameTimer;    // FPS for the status bar
    UIContext ui;             // widgets and mouse routing
    bool is_running;
};
```

`Application` is a namespace of free functions, not a class. Each `application_*.cpp` file implements one part of it.

## Frame loop

[application.cpp](../src/application/application.cpp):

```
while running:
    input.beginFrame()          // copy current key/mouse state to "previous", zero deltas
    Platform::pollEvents()      // Win32 messages → Event structs → InputState
    ui.beginFrame(...)          // UI decides whether it owns the mouse this frame
    actions.setMouseBlocked(..) // if so, viewport mouse actions don't fire
    checkActions(ctx)           // read actions for active contexts, run tools, edit the scene
    renderFrame(ctx)            // upload dirty meshes, draw objects, grid, debug, UI (markers, status bar, widgets, console)
```

Rendering order inside `renderFrame`:
1. Clear (dark gray).
2. For each object: re-upload the GPU mesh if `meshDirty`, then draw faces → edges → vertices.
3. Ground grid and axes (blended, no depth writes).
4. Debug half-edge overlay, if the `Debug` context is on.
5. Console background and text, if the `Console` context is on.

## Input to edit: how data flows

```
WM_KEYDOWN ──► Window callback ──► EventDispatcher::trigger(Event::KeyDown)
                                          │
                       registerInputEvents subscriber
                                          ▼
                                     InputState::onKey
                                          │
checkActions ──► ActionMap::wasActionPressedThisFrame(Action::GrabSelection, input, context)
                                          │
                                          ▼
                    tool code edits MeshData, sets object.meshDirty
                                          │
renderFrame ──► OpenGLMesh::update(meshData) ──► draw
```

Key ideas:
- **Events** carry raw OS input. The app only subscribes to turn them into `InputState`.
- **Actions** are named intents (`Action::GrabSelection`) bound to key combos. Tools never check raw keys.
- **Input contexts** are bit flags saying which modes are active (vertex mode, grab, console…). An action only fires if one of its contexts is active. Modal tools switch the context so that, for example, left click means "confirm grab" during a grab.
- **Dirty flags**: mesh edits set `Object::meshDirty` (rebuild GPU buffers) and per-face `triangulationDirty` (re-triangulate that face).

## Modal tool pattern

Grab, scale, rotate, bevel, extrude, and inset all follow the same shape:

1. **Start** (in `checkSelectionContext`): save each selected vertex's start position, call `history.begin(scene)`, then `input_ctx.setContext(InputContext_Grab)` (or Scale/Rotate/Bevel).
2. **Each frame** (`checkGrabContext` etc.): move vertices from mouse deltas.
3. **Confirm**: `history.commit()` and go back to the selection context.
4. **Cancel**: restore start positions or the saved mesh, `history.cancel(scene)`, and go back.

See [systems/application.md](systems/application.md).

## Undo pattern

Any edit is wrapped in `History`:

```cpp
ctx.history.begin(ctx.scene);       // snapshot before changing anything
if (mesh.someOperation(...)) ctx.history.commit();
else                         ctx.history.cancel(ctx.scene);
```

Snapshots are full copies of every object's `MeshData`, `Transform`, and the `Selection`. See [systems/scene.md](systems/scene.md#history).

## Conventions

- Integer and float aliases from `include/types`: `u8`…`u64`, `i8`…`i64`, `f32`, `f64`, and `INVALID_INDEX`.
- Member variables use `m_` prefix; functions are camelCase; files are snake_case.
- Mesh elements are referred to by generational handles (`VertexHandle`, `EdgeHandle`, `FaceHandle`), never pointers.
- Mesh operators return `bool` or an invalid handle on failure and must leave the mesh valid either way.
- Comments are sparse, one line, and describe a step.
