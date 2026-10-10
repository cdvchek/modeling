# Architecture

A from-scratch C++20 modeling app on Win32 and OpenGL 3.3. No windowing, UI, or math libraries; GLAD is the only third-party code.

## Layers

```
          main.cpp
             │
       ┌─────▼──────┐
       │ application│  startup, main loop, tools (action_checks/), console commands, windows and menus
       └─────┬──────┘
   ┌─────────┼──────────┬──────────────┬───────────┬──────────────┐
┌──▼───┐ ┌───▼───┐  ┌───▼──┐   ┌─────▼────┐ ┌────▼─────┐ ┌──────▼───────┐
│ core │ │ scene │  │  ui  │   │ renderer │ │ platform │ │project, asset│
└──────┘ └───────┘  └──────┘   └──────────┘ └──────────┘ └──────────────┘
 events   objects   draw list   IRenderer    Win32 window   .vlm files
 input    mesh      widgets     OpenGL impl  message pump   .vlmobj baking
 console  selection modal       shaders      GL context     (shared/vlmobj:
 font     origins   windows     GPU meshes   key mapping     the format)
 math, io camera
 threads  history
```

| Directory | Role | Depends on |
|---|---|---|
| `src/core/` | Engine building blocks with no app knowledge: math, containers (`DynamicArray`), events, input, console, bitmap fonts, frame timing, binary reading/writing and CRC-32 (`io/`), `parallelFor` (`thread/`) | — |
| `src/scene/` | Everything being edited: objects, materials, lights, reference images, half-edge meshes (`mesh/`, editing operations in `mesh/ops/`, presets in `mesh/presets/`), selection, picking (`picking/`), camera, history | core |
| `src/project/` | The `.vlm` project file format: writing and reading a whole scene plus editor state (see [systems/project.md](systems/project.md)) | core (io, threads), scene, ui (`Rect`) |
| `src/asset/` | Valuma's side of `.vlmobj` assets: baking an object into a file and rebuilding one from it, and the export naming rules (see [systems/vlmobj.md](systems/vlmobj.md)) | scene, `shared/vlmobj` |
| `shared/` | Code for the whole suite (Valuma, and later the Aevora engine and Sollaria audio), with no dependency on any one program. Now: `shared/vlmobj/`, the asset format, and `shared/image/`, image files (PNG). | — (C++ standard library only) |
| `src/ui/` | 2D UI draw list in pixel coordinates (shapes, text, clipping) and the immediate-mode widget system (`UIContext`). No OpenGL. | core (math, fonts) |
| `src/renderer/` | Backend-neutral `IRenderer` interface plus the OpenGL implementation | core, scene (mesh handles, `Scene` for the debug overlay), ui (draws a `UIDrawList`) |
| `src/platform/` | Win32 window, message pump, key translation, OpenGL context creation | core (events, keys) |
| `src/application/` | Wires everything together and owns all modeling behavior triggered by input: `actions/` (and `actions/checks/`), `commands/` (console), `ui/` (panel, console view, menus), `viewport/` (GPU meshes, markers, material looks), `tools/` (see [systems/application.md](systems/application.md)) | everything |

**Platform split:** platform-specific code lives in files ending in `_win32.cpp`. The OpenGL renderer is split into `renderer/opengl/opengl_renderer_common.cpp` (portable GL drawing) and `platform/renderer/opengl_renderer_win32.cpp` (WGL context setup only). Shaders live in `renderer/opengl/shaders/` and have no platform code.

## Build targets

Defined in [CMakeLists.txt](../CMakeLists.txt):

| Target | Contents |
|---|---|
| `vlmobj` (static lib) | The `.vlmobj` format from `shared/vlmobj/`. Depends on nothing else, so the engine can link it too. `modeling_core` links it. |
| `image` (static lib) | The PNG reader and writer from `shared/image/` (see [systems/image.md](systems/image.md)). Depends on nothing else. `modeling_core` links it. |
| `modeling_core` (static lib) | Math, fonts, input (`InputState`, `ActionMap`, `ContextManager`), the console and command system, objects, materials, lights, reference images, selection and picking, transforms, the camera, undo history, the UI, all `MeshData` code, the project file format, and Valuma's `.vlmobj` baking (`src/asset/`). No OpenGL or Win32, so it can be tested on its own. |
| `modeling` (exe → `bin/modeling.exe`) | Everything else plus `glad.c`, linked with `opengl32`, `dwmapi`, and `comdlg32` (file dialogs; all three are part of Windows). |
| `tests` (exe) | Every `tests/*.cpp` and `shared/*/tests/*.cpp`, linked against `modeling_core`. `SOURCE_DIR` is defined so tests can find checked-in reference files. |

New `.cpp` files must be added to `CORE_SRC` or `APP_SRC` by hand. Tests are picked up by glob.

## The shared context

All state lives in one `AppContext` ([app_context.hpp](../src/application/app_context.hpp)), created in `main()` and passed by reference everywhere:

```cpp
struct AppContext {
    Systems systems;          // events, input, actions, input_ctx, console, commands
    std::unique_ptr<IRenderer> renderer;
    DebugRenderer debug_renderer;
    std::vector<std::unique_ptr<Window>> windows;   // only windows[0] is used
    Scene scene;              // camera, objects, materials, lights, reference images, selection
    WidthTool widthTool;      // state of an in-progress bevel or inset
    TransformTool transformTool; // scale/rotate pivot and angle on screen
    OriginEdit originEdit;    // an origin's start while grab or rotate moves it
    History history;          // undo/redo
    ViewportSettings viewport; // headlight and other view-only settings
    FontLibrary fonts;        // embedded bitmap fonts
    UIDrawList uiDrawList;    // 2D UI, rebuilt every frame
    FrameTimer frameTimer;    // FPS for the status bar
    FrameStats frameStats;    // CPU timings for the stats readout
    UIContext ui;             // widgets and mouse routing
    ObjectMeshCache objectMeshes; // GPU copies of object meshes, by handle
    PictureTextureCache pictureTextures; // GPU textures for pictures (reference images and textures)
    LayerTextureCache layerTextures;     // GPU textures for layered textures, updated where they change
    MaterialPreviewCache materialPreviews;   // material swatch textures
    ProjectState project;     // the open file, unsaved changes, and the export folder
    ModalState modal;         // the open modal window (prompt or Export window), if any
    WorkspaceState workspace; // the current workspace (Model, UV, or Paint), the UV split and editor view, Paint's view and texture
    RadialMenuState radialMenu;
    ConsoleViewState consoleView; // console scroll and clickable rows
    u32 lastEditMode;         // where Tab returns to from object mode
    bool importRequested;     // Import… was picked; the file dialog opens next frame
    bool referenceRequested;  // + in the References tab; likewise
    std::filesystem::path referenceFolder; // where the image dialog last picked from
    TextureRequest textureRequest;  // a texture's PNG dialog, and the material to put it on
    std::filesystem::path textureFolder;   // where the texture dialog last picked from
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
    checkActions(ctx)           // a modal window, or: radial menu, console, picking and camera, actions, tools
    updateWindowTitle(ctx)      // name.vlm* - Valuma Studio
    renderFrame(ctx)            // upload dirty meshes, draw objects, grid, debug, then the UI
```

Rendering order inside `renderFrame`:
1. Render any material swatches that changed (each into its own texture), then clear and draw the background gradient.
2. Reference images set to Behind (no depth test or writes, so everything draws over them).
3. For each object: re-upload the GPU mesh if `meshDirty` or its stamp changed, then draw faces (in its material, or clay) → edges → vertices. See-through objects wait.
4. See-through objects and reference images set to Scene, sorted together, farthest first, depth tested.
5. Ground grid and axes (blended, no depth writes).
6. Reference images set to Front, without the depth test.
7. Debug half-edge overlay, if the `Debug` context is on.
8. The UI, in one draw list: selected images' outlines, light markers, origin markers, tool guides, the status bar, the panel, an open modal window, the radial menu, then the console on top (see [systems/ui.md](systems/ui.md#how-a-frame-draws-ui)).

## Input to edit: how data flows

```
WM_KEYDOWN ──► Window callback ──► EventDispatcher::trigger(Event::KeyDown)
                                          │
                       registerInputEvents subscriber
                                          ▼
                                     InputState::onKey
                                          │
checkActions ──► ActionMap::dispatch ──► wasActionPressedThisFrame(Action::GrabSelection, input, context)
                                          │
                                          ▼
                          handler: canGrab → startGrab
                                          │
                                          ▼
                    tool code edits MeshData, sets object.meshDirty
                                          │
renderFrame ──► ObjectMeshCache::sync (OpenGLMesh::update) ──► draw
```

Key ideas:
- **Events** carry raw OS input. The app only subscribes to turn them into `InputState`.
- **Actions** are named intents (`Action::GrabSelection`) bound to key combos. Tools never check raw keys. One-shot actions also have a handler (label, `canRun`, `run`) that `ActionMap::dispatch` runs when their keys are pressed, so the radial menu runs exactly the same action as the key.
- **Input contexts** are bit flags saying which modes are active (vertex mode, grab, console…). An action only fires if one of its contexts is active. Modal tools switch the context so that, for example, left click means "confirm grab" during a grab.
- **Dirty flags**: mesh edits set `Object::meshDirty` (rebuild GPU buffers) and per-face `triangulationDirty` (re-triangulate that face).

## Modal tool pattern

Grab, scale, rotate, bevel, extrude, and inset all follow the same shape:

1. **Start** (an action handler such as `startGrab` in `editing_actions.cpp`, from the key or the radial menu): save the start positions (vertices, lights, objects, reference images, or an origin), call `history.begin(scene)`, then `input_ctx.setContext(InputContext_Grab)` (or Scale/Rotate/Bevel/Inset).
2. **Each frame** (`checkGrabContext` etc.): rebuild positions from the starts and the mouse.
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

Snapshots are full copies of every object (mesh and transform), every light and the ambient light, and the `Selection`. See [systems/scene.md](systems/scene.md#history).

## Conventions

- Integer and float aliases from `include/types`: `u8`…`u64`, `i8`…`i64`, `f32`, `f64`, and `INVALID_INDEX`.
- Member variables use `m_` prefix; functions are camelCase; files are snake_case.
- Mesh elements are referred to by generational handles (`VertexHandle`, `EdgeHandle`, `FaceHandle`), never pointers.
- Mesh operators return `bool` or an invalid handle on failure and must leave the mesh valid either way.
- Comments are sparse, one line, and describe a step.
