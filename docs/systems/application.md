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
| [application_actions.cpp](../../src/application/application_actions.cpp) | `registerDefaultActions`: keybinds, plus a handler (label, `canRun`, `run`) for each one-shot action |
| [editing_actions.cpp](../../src/application/editing_actions.cpp) | The one-shot actions as named functions, with `can*` checks: `setSelectionMode` (object mode selects the active object; edit modes are remembered in `ctx.lastEditMode`), `toggleObjectMode` (Tab), `toggleAxis`, `undo`/`redo`, `startGrab`/`startScale`/`startRotate`, `extrudeSelection` (inset starts from `beginInset` in check_inset.cpp), `connectVertices`, `fillFaceLoop`, `mergeVertices`, `dissolveSelection` (also used by the `merge` and `dissolve` commands), `deleteSelection` and `deleteSelectedObjects`, `clearAxes`, `setLightType`, `toggleLights`, and the view toggles `togglePanel`, `toggleHeadlight`, `toggleDebugView` (not undoable). Run by the action registry (see [input.md](input.md#handlers-the-action-registry)) |
| [tool_guides.cpp](../../src/application/tool_guides.cpp) | `initTransformTool`: on the first scale/rotate update, projects the selection center to the screen and stores it with the mouse's start distance and angle in `ctx.transformTool` ([transform_tool.hpp](../../src/application/transform_tool.hpp)); `startScale` and `startRotate` reset it. `drawToolGuides`: a faint line (`UIStyle::GUIDE_LINE`, the light markers' drop line style) with a dot at its start, both drawn over a slightly wider dark halo (`UIStyle::GUIDE_HALO`) so they read on light faces too, from the pivot to the mouse during scale/rotate, or from the start point to the mouse during bevel and inset. |
| [console_view.cpp](../../src/application/console_view.cpp) | `drawConsole`: the console panel docked above the status bar, with commands, output, and errors above an input line with a blinking caret; `updateConsoleView`: wheel scrolling and click-to-fold (see [console.md](console.md)) |
| [radial_menu.cpp](../../src/application/radial_menu.cpp) | The radial menu. `buildRadialMenu(ctx, id)` returns a menu's items (action, submenu, or dimmed placeholder) in clockwise order from the top, chosen by context. `updateRadialMenu` opens it on the RadialMenu action, tracks the hovered slice with `RadialLayout::sliceAt`, re-centers into a submenu once the cursor passes `SUBMENU_RADIUS`, and on release runs the hovered item's handler if `ActionMap::isAvailable`. `drawRadialMenu` draws ring slices, label boxes with faint key hints, and a center dot. State is `ctx.radialMenu` ([radial_menu_state.hpp](../../src/application/radial_menu_state.hpp)). Sizes and colors are constants at the top. |
| [application_commands.cpp](../../src/application/application_commands.cpp) | `registerCommands` (see [console.md](console.md)) |
| [main_panel.cpp](../../src/application/main_panel.cpp) | `drawMainPanel`: the floating panel (shown by default, `ui panel` toggles it), with two tabs. Placed at the top right on first use; kept inside the viewport above the status bar (`statusBarHeight`). **Objects tab:** a header row with a preset dropdown (stored in `ctx.viewport.newObjectPreset`), + (adds that preset with `addObject`) and − (removes the active object with `removeObject`); a fixed-height object list (click a row to edit that object; in object mode the row highlight shows the selected objects and a click selects just that one; detail shows the face count); the selected object's name, vertex/edge/face counts, and Position, Rotation (degrees), and Scale fields (undoable; scale is kept at least 0.001 from zero). The selected object and selected light each start with a **Name** text field (renames as one undo step). **Lights tab:** a header row with a type button (cycles point → spot → directional; `ctx.viewport.newLightType`), + and −; a fixed-height light list; the selected light's properties; the ambient light; the headlight. Each section has its own ID scope. |
| [ui_undo.hpp](../../src/application/ui_undo.hpp) | `trackUndo`: makes one widget interaction one undo step |
| [status_bar.cpp](../../src/application/status_bar.cpp) | `drawStatusBar`: the bar along the bottom of the viewport. Items are laid out left to right with a divider between them. Each item reserves the width of its longest possible value (e.g. `MODE_NAMES`, `TOOL_NAMES`), so switching modes or tools never moves the items after it. An item is a list of colored text segments, which is how the axis lock shows a dim `Axis` label followed by `X`/`Y`/`Z` in red/green/blue (several can be on at once, since each axis toggles independently). Add one by appending a `StatusItem` to `items`. The first item is the frame rate from `ctx.frameTimer` (ticked at the start of every `renderFrame`). `selectionModeName` reads the remembered selection mode (correct even during a modal tool) and `activeToolName` the modal tool, both from the input contexts. When `Console::getErrorCount()` goes up while the console is closed, the first line of `getLatestError()` shows right-aligned in red (`! ...`) for `ERROR_SHOWN` (4 s), fading out over the last `ERROR_FADE` (0.6 s), cut short with `...` if it would run into the items; errors that arrive while the console is open don't flash. |
| [light_markers.cpp](../../src/application/light_markers.cpp) | `drawLightMarkers`: projects each light to the screen and draws it with the UI draw list, so markers are a fixed pixel size and always on top of the scene. Orb in the light's color with a dark outline and soft glow (disabled: hollow gray ring); spot lights add an arrow along their direction and directional lights three parallel arrows (the outer two shorter). Arrows are built in world space and projected, so they foreshorten in 3D; their lengths are given in pixels and converted to world units at the light's distance so they stay a steady size on screen. Arrowheads have four fins (an X when seen end-on), and the side arrows are spread across the view; a faint line drops to the grid with a dot where it lands. Selected lights get a purple ring right against the orb's dark outline, with a soft purple glow behind it (the orb keeps its color). Farther lights are drawn first. Lights behind the camera are skipped. Sizes and colors are constants at the top of the file. |
| [object_commands.cpp](../../src/application/object_commands.cpp) | `runObjectCommand`: everything behind the `object` command. `objectPresets()`, `addObject`, and `removeObject` are shared with the Objects tab. |
| [object_meshes.cpp](../../src/application/object_meshes.cpp) | `ObjectMeshCache`: one `OpenGLMesh` per object slot. `sync` creates it for a new object (or a new object in a reused slot), rebuilds it when `meshDirty`, and clears the flag; `prune` frees meshes of removed objects. Called from `renderFrame`. |
| [light_commands.cpp](../../src/application/light_commands.cpp) | `runLightCommand`: everything behind the `light` command. `deleteSelectedLights`: removes the selected lights as one undo step (used by the panel's − button and the Delete key) |
| [command_parsing.hpp](../../src/application/command_parsing.hpp) | Argument parsers shared by commands: `parseFloat`, `parseUnitFloat` (0..1), `parseU32`, `parseVec3Args` |
| [application_scene.cpp](../../src/application/application_scene.cpp) | `initializeCamera`, `loadTestScene` |
| `action_checks/` | Per-context input handlers: the tools |
| [width_tool.hpp](../../src/application/width_tool.hpp) | State kept while a bevel is in progress |

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
| always | Quit | `check_actions.cpp` |
| always | `updateRadialMenu`; while the menu is open (or opened/closed this frame) nothing below runs | `radial_menu.cpp` |
| always | ToggleConsole | `check_actions.cpp` |
| Console | `checkConsoleContext` | `check_console.cpp` |
| any selection mode, object mode included | `checkSelectionContext` | `check_selection.cpp` |
| always | `ActionMap::dispatch`: one-shot actions (modes, starting tools, operators, undo/redo, X/Y/Z axis locks) | `editing_actions.cpp` |
| Grab | `checkGrabContext` | `check_grab.cpp` |
| Scale | `checkScaleContext` | `check_scale.cpp` |
| Rotate | `checkRotateContext` | `check_rotate.cpp` |
| Bevel | `checkBevelContext` | `check_bevel.cpp` |
| Inset | `checkInsetContext` | `check_inset.cpp` |

### checkSelectionContext

The continuous editing input, in order:
1. Camera orbit, pan, zoom.
2. Picking: builds a ray from the mouse with `makeRayFromScreenPosition`, then calls `pickVertex`/`pickEdge`/`pickFace` for the current mode. Loop and ring selection use `MeshData::getEdgeLoop`, `getEdgeRing`, and `getFaceLoop`. Face loops run across the edge of the clicked face nearest the click point.

In object mode a click picks the nearest object with `pickFace` (any object), selecting it and making it active; Shift+click toggles it. Lights are picked first in every mode, and picking a light clears selected objects.

Mode switches (M+V / M+E / M+F / M+O and Tab, which clear the selection), starting tools, one-shot operators, and undo/redo run afterwards through `ActionMap::dispatch`.

Helpers in the file's anonymous namespace keep vertex selection in sync: `selectEdge` and `selectFace` also select their vertices; `deselectEdge` and `deselectFace` drop vertices no other selected element uses. Edges are treated as selected if either half-edge is.

## Modal tools

All follow the pattern in [architecture.md](../architecture.md#modal-tool-pattern): save start positions → `history.begin` → `setContext(tool)` → update each frame → confirm (`commit`) or cancel (restore + `cancel`) → `setContext(getSelectionContext())`.

In object mode the same three tools work on whole objects: `startGrab`/`startScale`/`startRotate` also save the selected objects' transforms (`setObjectStartTransforms`). Grab adds the mouse movement to each position; scale moves positions away from the objects' shared center and multiplies their scale (kept at least 0.001 from zero; an axis lock limits both to the locked components); rotate turns positions around the shared center and composes the turn into each object's Euler rotation with `rotateEuler`. Cancel restores the saved transforms. Undo and redo drop selection that belongs to the other kind of mode (objects in edit modes, elements in object mode).

Vertex positions are in the active object's mesh space, but grab, scale, and rotate do their math in world space through `ObjectSpace` (see [scene.md](scene.md#transform)): grab converts the world movement with `directionToLocal`, and scale and rotate convert start positions to world, transform them, and convert back. So on a rotated, moved, or scaled object the selection follows the mouse and axis locks are world axes.

Start positions are stored on the selection with `Selection::setSelectionStartPositions`, in the same order as `getVertices()`.

### Grab — `checkGrabContext`
- Moves selected vertices and selected lights by mouse delta along the camera's right and up vectors. Speed is `0.001 × camera.distance` per pixel. G starts it when vertices or lights are selected; their start positions are saved then (`setSelectionStartPositions`, `setLightStartPositions`).
- With an axis lock on, movement is zeroed on the other axes. When a lock is first turned on, vertices and lights snap back onto that axis through their start positions.
- Cancel puts every vertex and light back at its start position.

### Scale — `checkScaleContext`
- Needs 2+ selected vertices.
- Scale factor = (mouse distance from the pivot on screen) ÷ (that distance when the tool started). The pivot is the average of the start positions, projected once on the first update (`initTransformTool`).
- Each frame rebuilds positions from the start positions: `center + (start − center) × factor`. With an axis lock, only the locked components scale; the others keep their start values.

### Rotate — `checkRotateContext`
- Needs 2+ selected vertices, or any selected lights. Light directions are saved when R is pressed (`setLightStartDirections`).
- The angle follows the mouse around the pivot on screen: each frame adds the change in the mouse's angle around it (wrapped to ±π with `wrapAngle`), so circling more than once keeps turning. Counterclockwise on screen turns the selection counterclockwise as you see it.
- Each frame rebuilds from the start positions and start light directions. Vertices turn around the selection center; lights turn in place (only their direction rotates, renormalized; their position stays). With only lights selected, the pivot is their average position.
- Axis: camera forward by default, or world X/Y/Z when locked, flipped to point toward the viewer so the turn matches the mouse. Cancel restores start positions and light directions.

### Bevel — `beginBevel`, `checkBevelContext`
`ctx.widthTool` ([width_tool.hpp](../../src/application/width_tool.hpp)) holds the state for bevel and inset.
- Needs exactly one selected vertex, edge, or face in the matching mode.
- `beginBevel` calls `MeshData::bevelVertex/Edge/Face` with `ctx.widthTool.session` and the object's matrix (so the bevel is measured in world space and the mouse distance converts straight to width), records a pivot (the element's center) and the mouse position, saves and clears the selection, and enters `Bevel` context.
- Each frame, the width is how far the mouse has moved from where the bevel started (in any direction), converted from pixels to world units at the pivot's depth. Moving back to the start brings it to zero. A dot marks the start point with a line to the mouse. Passed to `MeshData::setSlideWidth`, which clamps it.
- Cancel calls `MeshData::cancelSlide` and restores the saved selection.
- Fails (prints a message, cancels history) on corners it can't rebuild, such as a vertex where two separate open edges meet.

### Extrude — `extrudeSelection`
Calls `MeshData::extrudeRegions` on every selected face, selects the moved top faces and their vertices, and enters **Grab**. The history step begun here is committed or cancelled by the grab. If the selection is refused, history is cancelled and the reason is printed (`extrude: ...`).

### Inset — `beginInset`, `checkInsetContext`
- `beginInset` (check_inset.cpp) calls `MeshData::insetRegions` with `ctx.widthTool.session` and the object's matrix, selects the new inner faces and their vertices (saving the old selection), records the pivot (the inner faces' center) and the mouse position, and enters the `Inset` context. A refused selection cancels history and prints `inset: ...`.
- Each frame runs `updateWidthTool` (shared with bevel, in check_bevel.cpp): the width is the mouse's distance from its start, in world units at the pivot's depth. Confirm keeps the inner faces selected; cancel restores the mesh and the old selection.

## One-shot operators

Each wraps a mesh call in `history.begin` / `commit` or `cancel`, then sets `meshDirty`:

| Key | Mode | Calls |
|---|---|---|
| C | vertex, 2 selected | `MeshData::connectVertices(a, b)` |
| F | edge, 1+ selected | `MeshData::fillFaceLoop(firstEdge)` |
| Delete | any | `removeVertex` / `removeEdge` / `removeFace` for each selected element, then clears the selection |

All of these, and the modal tools, act on the active object (`ctx.scene.activeObject()`).

## Adding a tool

1. Add actions and keybinds (see [input.md](input.md#adding-an-action)).
2. If it's modal, add an `InputContext_*` bit, a `check*Context` function declared in `action_checks.hpp`, and a dispatch line in `checkActions`.
3. Write a `start*` function (and `can*` check) in `editing_actions.cpp` following the save → `history.begin` → `setContext` pattern, and `setHandler` it in `registerDefaultActions`.
4. Put the geometry change in `MeshData` (so it's testable in `modeling_core`), not in the tool.
5. Document it here, in [features.md](../features.md), and the mesh operator in [mesh.md](mesh.md).
