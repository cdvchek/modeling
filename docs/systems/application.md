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
| [main_panel.cpp](../../src/application/main_panel.cpp) | `drawMainPanel`: the floating panel (shown by default, `ui panel` toggles it). Placed at the top right on first use; kept inside the viewport above the status bar (`statusBarHeight`). Contents for now: ambient, headlight, light list, add light, selected light's properties. See [ui.md](ui.md#floating-panel). |
| [ui_undo.hpp](../../src/application/ui_undo.hpp) | `trackUndo`: makes one widget interaction one undo step |
| [status_bar.cpp](../../src/application/status_bar.cpp) | `drawStatusBar`: the bar along the bottom of the viewport. Items are laid out left to right with a divider between them. Each item reserves the width of its longest possible value (e.g. `MODE_NAMES`, `TOOL_NAMES`), so switching modes or tools never moves the items after it. An item is a list of colored text segments, which is how the axis lock shows a dim `Axis` label followed by `X`/`Y`/`Z` in red/green/blue (several can be on at once, since each axis toggles independently). Add one by appending a `StatusItem` to `items`. The first item is the frame rate from `ctx.frameTimer` (ticked at the start of every `renderFrame`). `selectionModeName` reads the remembered selection mode (correct even during a modal tool) and `activeToolName` the modal tool, both from the input contexts. |
| [light_markers.cpp](../../src/application/light_markers.cpp) | `drawLightMarkers`: projects each light to the screen and draws it with the UI draw list, so markers are a fixed pixel size and always on top of the scene. Orb in the light's color with a dark outline and soft glow (disabled: hollow gray ring); spot lights add an arrow along their direction and directional lights three parallel arrows (the outer two shorter). Arrows are built in world space and projected, so they foreshorten in 3D; their lengths are given in pixels and converted to world units at the light's distance so they stay a steady size on screen. Arrowheads have four fins (an X when seen end-on), and the side arrows are spread across the view; a faint line drops to the grid with a dot where it lands. Selected lights get a warm amber ring right against the orb's dark outline, with a soft amber glow behind it (the orb keeps its color). Farther lights are drawn first. Lights behind the camera are skipped. Sizes and colors are constants at the top of the file. |
| [light_commands.cpp](../../src/application/light_commands.cpp) | `runLightCommand`: everything behind the `light` command |
| [command_parsing.hpp](../../src/application/command_parsing.hpp) | Argument parsers shared by commands: `parseFloat`, `parseUnitFloat` (0..1), `parseU32`, `parseVec3Args` |
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
- Moves selected vertices and selected lights by mouse delta along the camera's right and up vectors. Speed is `0.001 × camera.distance` per pixel. G starts it when vertices or lights are selected; their start positions are saved then (`setSelectionStartPositions`, `setLightStartPositions`).
- With an axis lock on, movement is zeroed on the other axes. When a lock is first turned on, vertices and lights snap back onto that axis through their start positions.
- Cancel puts every vertex and light back at its start position.

### Scale — `checkScaleContext`
- Needs 2+ selected vertices.
- Scale factor each frame = (mouse distance from screen center after the move) ÷ (before the move).
- Scales around the average of the start positions. Axis locks restrict which components scale.

### Rotate — `checkRotateContext`
- Needs 2+ selected vertices, or any selected lights. Light directions are saved when R is pressed (`setLightStartDirections`).
- Horizontal mouse movement rotates by `0.002` rad per pixel. Vertices turn around the selection center; lights turn in place (only their direction rotates, renormalized; their position stays).
- Axis: camera forward by default, or world X/Y/Z when locked. Turning on a lock first restores start positions and light directions; so does cancel.

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
