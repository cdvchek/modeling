# Application

Startup, the main loop, and all modeling behavior triggered by input. This is the only layer that knows about every other system.

Files: `src/application/`

| File | Contents |
|---|---|
| [application.hpp](../../src/application/application.hpp) | `namespace Application` function list |
| [app_context.hpp](../../src/application/app_context.hpp), [app_systems.hpp](../../src/application/app_systems.hpp) | `AppContext` and `Systems` (see [architecture.md](../architecture.md#the-shared-context)) |
| [viewport_settings.hpp](../../src/application/viewport_settings.hpp) | `ViewportSettings`: view-only settings that aren't scene data or undoable. Holds the `Headlight` (`enabled`, `color`, `strength`, default on at 0.4). |
| [application.cpp](../../src/application/application.cpp) | `initialize`, `run`, `renderFrame` |
| [application_render.cpp](../../src/application/application_render.cpp) | `createMainWindow`, `setupRenderer` |
| [application_events.cpp](../../src/application/application_events.cpp) | `registerInputEvents` |
| [application_actions.cpp](../../src/application/application_actions.cpp) | `registerDefaultActions` |
| [application_commands.cpp](../../src/application/application_commands.cpp) | `registerCommands` (see [console.md](console.md)) |
| [application_scene.cpp](../../src/application/application_scene.cpp) | `initializeCamera`, `loadTestScene` |
| `action_checks/` | Per-context input handlers: the tools |
| [bevel_tool.hpp](../../src/application/bevel_tool.hpp) | State kept while a bevel is in progress |

## Startup

`Application::initialize(ctx)`:
1. `createMainWindow` — 1920×1080 Win32 window.
2. `setupRenderer` — creates the OpenGL renderer with VSync on.
3. `registerInputEvents`, `registerDefaultActions`, `registerCommands`.
4. `initializeCamera` — camera at (0, 0, 3) looking at the origin, Y up.
5. `loadTestScene` — creates one object, `"cube"`, and uploads its GPU mesh.
6. Starts in vertex selection mode.

## Main loop

`Application::run` loops `input.beginFrame()` → `Platform::pollEvents()` → `checkActions()` → `renderFrame()` until `is_running` is false. See [architecture.md](../architecture.md#frame-loop).

`renderFrame` builds a `DrawCommand` per object (with the selection as highlights), then a `DrawGridCommand`, then the debug overlay and console if active. Vertex points are only shown in vertex mode.

## Action checks

`Application::checkActions` ([check_actions.cpp](../../src/application/action_checks/check_actions.cpp)) runs every frame and dispatches by active context:

| Context active | Handler | File |
|---|---|---|
| always | Quit, ToggleConsole | `check_actions.cpp` |
| Console | `checkConsoleContext` | `check_console.cpp` |
| any selection mode | `checkSelectionContext` | `check_selection.cpp` |
| Grab / Scale / Rotate | X/Y/Z axis toggles | `check_actions.cpp` |
| Grab | `checkGrabContext` | `check_grab.cpp` |
| Scale | `checkScaleContext` | `check_scale.cpp` |
| Rotate | `checkRotateContext` | `check_rotate.cpp` |
| Bevel | `checkBevelContext` | `check_bevel.cpp` |

### checkSelectionContext

The main editing handler. In order:
1. Undo / redo.
2. Mode switch (M+V / M+E / M+F); clears the selection.
3. Camera orbit, pan, zoom.
4. Picking: builds a ray from the mouse with `makeRayFromScreenPosition`, then calls `pickVertex`/`pickEdge`/`pickFace` for the current mode. Loop and ring selection use `MeshData::getEdgeLoop`, `getEdgeRing`, and `getFaceLoop`. Face loops run across the edge of the clicked face nearest the click point.
5. Starting tools: grab, scale, rotate, bevel (see below).
6. One-shot operators: connect (vertex mode), fill (edge mode), extrude/inset (face mode), delete.

Helpers in the file's anonymous namespace keep vertex selection in sync: `selectEdge` and `selectFace` also select their vertices; `deselectEdge` and `deselectFace` drop vertices no other selected element uses. Edges are treated as selected if either half-edge is.

## Modal tools

All follow the pattern in [architecture.md](../architecture.md#modal-tool-pattern): save start positions → `history.begin` → `setContext(tool)` → update each frame → confirm (`commit`) or cancel (restore + `cancel`) → `setContext(getSelectionContext())`.

Start positions are stored on the selection with `Selection::setSelectionStartPositions`, in the same order as `getVertices()`.

### Grab — `checkGrabContext`
- Moves selected vertices by mouse delta along the camera's right and up vectors. Speed is `0.001 × camera.distance` per pixel.
- With an axis lock on, movement is zeroed on the other axes. When a lock is first turned on, vertices snap back onto that axis through their start positions.
- Cancel puts every vertex back at its start position.

### Scale — `checkScaleContext`
- Needs 2+ selected vertices.
- Scale factor each frame = (mouse distance from screen center after the move) ÷ (before the move).
- Scales around the average of the start positions. Axis locks restrict which components scale.

### Rotate — `checkRotateContext`
- Needs 2+ selected vertices.
- Horizontal mouse movement rotates by `0.002` rad per pixel around the selection center.
- Axis: camera forward by default, or world X/Y/Z when locked. Turning on a lock first restores start positions.

### Bevel — `beginBevel`, `checkBevelContext`
- Needs exactly one selected vertex, edge, or face in the matching mode.
- `beginBevel` calls `MeshData::bevelVertex/Edge/Face` with `ctx.bevel.session`, records a pivot (the element's center) and the starting mouse distance from the pivot on screen, saves and clears the selection, and enters `Bevel` context.
- Each frame, the width is how much farther the mouse is from the pivot than at the start, converted from pixels to world units at the pivot's depth. Passed to `MeshData::setBevelWidth`, which clamps it.
- Cancel calls `MeshData::cancelBevel` and restores the saved selection.
- Fails (prints a message, cancels history) when the geometry touches a border.

### Extrude and inset
Both call `MeshData::insertFaceRing` on the first selected face, select the new top face and its vertices, and then enter **Grab** (extrude) or **Scale** (inset). The history step begun here is committed or cancelled by that tool.

## One-shot operators

Each wraps a mesh call in `history.begin` / `commit` or `cancel`, then sets `meshDirty`:

| Key | Mode | Calls |
|---|---|---|
| C | vertex, 2 selected | `MeshData::connectVertices(a, b)` |
| F | edge, 1+ selected | `MeshData::fillFaceLoop(firstEdge)` |
| Delete | any | `removeVertex` / `removeEdge` / `removeFace` for each selected element, then clears the selection |

Most of these operate on `ctx.scene.objects.get(0)` directly. Fixing that is required before multiple objects can be edited.

## Adding a tool

1. Add actions and keybinds (see [input.md](input.md#adding-an-action)).
2. If it's modal, add an `InputContext_*` bit, a `check*Context` function declared in `action_checks.hpp`, and a dispatch line in `checkActions`.
3. Start it from `checkSelectionContext`, following the save → `history.begin` → `setContext` pattern.
4. Put the geometry change in `MeshData` (so it's testable in `modeling_core`), not in the tool.
5. Document it here, in [features.md](../features.md), and the mesh operator in [mesh.md](mesh.md).
