# Application

Startup, the main loop, and all modeling behavior triggered by input. This is the only layer that knows about every other system.

Files: `src/application/`, by folder:

- the root: the app itself (`application*.cpp`: startup, the frame, input wiring, rendering, commands registration), `AppContext`, `AppSystems`, `ProjectState`
- `actions/`: what the user does to the scene (editing, origins, saving and opening, export and import); `actions/checks/` reads input each frame and starts or drives tools
- `commands/`: console commands (`object`, `light`, `material`, `reference`, `shading`) and argument parsing
- `ui/`: the app's own interface: the panel, console view, modal windows, radial menu, status bar, stats overlay
- `viewport/`: what feeds the 3D view: GPU mesh cache, material looks, light and origin markers, reference images, view settings
- `tools/`: state and guide lines for grab/scale/rotate and bevel/inset

| File | Contents |
|---|---|
| [application.hpp](../../src/application/application.hpp) | `namespace Application` function list |
| [app_context.hpp](../../src/application/app_context.hpp), [app_systems.hpp](../../src/application/app_systems.hpp) | `AppContext` and `Systems` (see [architecture.md](../architecture.md#the-shared-context)) |
| [viewport_settings.hpp](../../src/application/viewport/viewport_settings.hpp) | `ViewportSettings`: view-only settings that aren't scene data or undoable: the `Headlight` (`enabled`, `color`, `strength`, default on at 0.8), `exposure` (stops, −5 to 5, `MIN_EXPOSURE`/`MAX_EXPOSURE`), whether the panel and the origins show, the panel's state, and what the Lights and Objects tabs' + buttons add. |
| [application.cpp](../../src/application/application.cpp) | `initialize`, `run`, `renderFrame` |
| [application_render.cpp](../../src/application/application_render.cpp) | `createMainWindow`, `setupRenderer` |
| [application_events.cpp](../../src/application/application_events.cpp) | `registerInputEvents` |
| [application_actions.cpp](../../src/application/application_actions.cpp) | `registerDefaultActions`: keybinds, plus a handler (label, `canRun`, `run`) for each one-shot action |
| [editing_actions.cpp](../../src/application/actions/editing_actions.cpp) | The one-shot actions as named functions, with `can*` checks: `setSelectionMode` (object mode selects the active object; edit modes are remembered in `ctx.lastEditMode`), `toggleObjectMode` (Tab), `toggleAxis`, `undo`/`redo`, `startGrab`/`startScale`/`startRotate`, `extrudeSelection` (inset starts from `beginInset` in check_inset.cpp), `connectVertices`, `fillFaceLoop`, `mergeVertices`, `dissolveSelection` (also used by the `merge` and `dissolve` commands), `deleteSelection` and `deleteSelectedObjects`, `clearAxes`, `setLightType`, `toggleLights`, `parentToActive` (Ctrl+P) and `clearParents` (Alt+P), and the view toggles `togglePanel`, `toggleHeadlight`, `toggleDebugView`, `toggleMaterials` (material view ↔ clay view) (not undoable). Run by the action registry (see [input.md](input.md#handlers-the-action-registry)) |
| [tool_guides.cpp](../../src/application/tools/tool_guides.cpp) | `initTransformTool`: on the first scale/rotate update, projects the selection center to the screen and stores it with the mouse's start distance and angle in `ctx.transformTool` ([transform_tool.hpp](../../src/application/tools/transform_tool.hpp)); `startScale` and `startRotate` reset it. `drawToolGuides`: a faint line (`UIStyle::GUIDE_LINE`, the light markers' drop line style) with a dot at its start, both drawn over a slightly wider dark halo (`UIStyle::GUIDE_HALO`) so they read on light faces too, from the pivot to the mouse during scale/rotate, or from the start point to the mouse during bevel and inset. |
| [console_view.cpp](../../src/application/ui/console_view.cpp) | `drawConsole`: the console panel docked above the status bar, with commands, output, and errors above an input line with a blinking caret; `updateConsoleView`: wheel scrolling and click-to-fold (see [console.md](console.md)) |
| [radial_menu.cpp](../../src/application/ui/radial_menu.cpp) | The radial menu. The root menu is the axis menu while a tool runs, the origin menu while an origin is selected, otherwise the main menu. `buildRadialMenu(ctx, id)` returns a menu's items (action, submenu, or dimmed placeholder) in clockwise order from the top, chosen by context. `updateRadialMenu` opens it on the RadialMenu action, tracks the hovered slice with `RadialLayout::sliceAt`, re-centers into a submenu once the cursor passes `SUBMENU_RADIUS`, and on release runs the hovered item's handler if `ActionMap::isAvailable`. `drawRadialMenu` draws ring slices, label boxes with faint key hints, and a center dot. State is `ctx.radialMenu` ([radial_menu_state.hpp](../../src/application/ui/radial_menu_state.hpp)). Sizes and colors are constants at the top. |
| [application_commands.cpp](../../src/application/application_commands.cpp) | `registerCommands` (see [console.md](console.md)) |
| [modal_windows.cpp](../../src/application/ui/modal_windows.cpp), [modal_state.hpp](../../src/application/ui/modal_state.hpp) | Modal windows (see [below](#modal-windows)): `showPrompt`, `openModal`, `closeModal`, `confirmModal`/`cancelModal` (Enter/Escape), `drawModal`, `updateModal`. State is `ctx.modal`. |
| [asset_actions.cpp](../../src/application/actions/asset_actions.cpp) | The Export window (`openExportWindow`, `drawExportWindow`, `updateExportWindow`, `confirmExportWindow`), `exportFolder`, and import (`importAssets`, `importAssetFrom`). See [vlmobj.md](vlmobj.md#in-the-app). |
| [project_actions.cpp](../../src/application/actions/project_actions.cpp) | Saving, opening, and new projects: `saveProject` (Ctrl+S), `saveProjectAs`, `saveProjectTo`, `openProject`, `openProjectFrom`, `newProject`, `confirmDiscardChanges(ctx, then)` (the unsaved-changes prompt; runs `then` after Save or Don't Save), `nativeWindow` and `afterDialog` (for native dialogs), `projectsFolder` (`Documents\Valuma Studio`) and `resolveProjectPath` (console paths), `updateWindowTitle`, `initializeProject`. State is `ctx.project` ([project_state.hpp](../../src/application/project_state.hpp)): the file path, the history state id at the last save, the startup view, and the title last set. See [project.md](project.md). |
| [main_panel.cpp](../../src/application/ui/main_panel.cpp) | `drawMainPanel`: the floating panel (shown by default, `ui panel` toggles it), with two tabs. Placed at the top right on first use; kept inside the viewport above the status bar (`statusBarHeight`). **Objects tab:** a header row with a preset dropdown (stored in `ctx.viewport.newObjectPreset`; its last entry is Import…), + (adds that preset with `addObject`, or with Import… picks `.vlmobj` files) and − (removes the active object with `removeObject`); a fixed-height object tree (`hierarchy()` with `treeRow`: children indented, an arrow folds a family away, folded objects kept in `ctx.viewport.foldedObjects`; click a row to edit that object; in object mode the row highlight shows the selected objects and a click selects just that one; detail shows the face count; drag a row onto another to parent it there, or onto the list's empty space to clear its parent, through `setObjectParent`); the selected object's name, vertex/edge/face counts, and Position, Rotation (degrees), and Scale fields, relative to the parent (undoable; scale is kept at least 0.001 from zero). The selected object and selected light each start with a **Name** text field (renames as one undo step). **Lights tab:** a header row with a type button (cycles point → spot → directional; `ctx.viewport.newLightType`), + and −; a fixed-height light list; the selected light's properties; the ambient light; the headlight; exposure. **Materials tab:** + (a new material from the defaults, "Material", numbered) and − (removes the selected one; dimmed for Default); a fixed-height list with each material's swatch and how many objects use it ("unused" for none); the selected material (`ctx.viewport.selectedMaterial`, a view setting, not undoable): a large swatch, its name (Default's can't change), Base color and Emissive (`colorEdit`), Roughness, Metallic, Glow (emissive strength, 0 to 10), Alpha (Opaque / Cutout / Blend), Opacity (not for Opaque), Cutoff (Cutout only), Both sides (double-sided), how many objects use it, and **Assign** (to the selected faces in face mode, with **Use object's material** beside it to clear them; otherwise the targets of `materialTargets`), **Select its faces** in face mode, each change one undo step. The **Objects tab** also gets a **Material** dropdown with swatches for the selected object, and a note when some of its faces have their own, then a **Shading** dropdown (Flat / Smooth / Auto, one undo step) and, for Auto, an **Angle** field in degrees (0 to 180, one undo step per drag). **Images tab:** + (sets `ctx.referenceRequested`) and − (deletes the selected images); a fixed-height list (any image can be picked there, locked ones included; rows say "locked" or "hidden"); the selected image's name, file and pixel size, Visible, Locked, Opacity, Depth (Behind / Scene / Front), Position, Rotation, and Size (`dragFloat`, at least `MIN_REFERENCE_SIZE`), each change one undo step. Each section has its own ID scope. |
| [ui_undo.hpp](../../src/application/ui/ui_undo.hpp) | `trackUndo`: makes one widget interaction one undo step |
| [status_bar.cpp](../../src/application/ui/status_bar.cpp) | `drawStatusBar`: the bar along the bottom of the viewport. Items are laid out left to right with a divider between them. Each item reserves the width of its longest possible value (e.g. `MODE_NAMES`, `TOOL_NAMES`), so switching modes or tools never moves the items after it. An item is a list of colored text segments, which is how the axis lock shows a dim `Axis` label followed by `X`/`Y`/`Z` in red/green/blue (several can be on at once, since each axis toggles independently). Add one by appending a `StatusItem` to `items`. The first item is the frame rate from `ctx.frameTimer` (ticked at the start of every `renderFrame`). `selectionModeName` reads the remembered selection mode (correct even during a modal tool) and `activeToolName` the modal tool, both from the input contexts. When `Console::getErrorCount()` goes up while the console is closed, the first line of `getLatestError()` shows right-aligned in red (`! ...`) for `ERROR_SHOWN` (4 s), fading out over the last `ERROR_FADE` (0.6 s), cut short with `...` if it would run into the items; errors that arrive while the console is open don't flash. |
| [origin_markers.cpp](../../src/application/viewport/origin_markers.cpp) | `drawParentLines` (object mode only): a faint dashed line from each child's origin to its parent's. `drawOriginMarkers`: each object's origin as a dot with a dark outline, drawn after the light markers (on top): the active object's bright, the others greyed, the selected one purple and larger with lines along its own +X, +Y, +Z (rotation only, a fixed 34 px on screen, over dark halos). Skipped when `ctx.viewport.showOrigins` is off. `ORIGIN_MARKER_PICK_RADIUS` (9 px) is the click radius. |
| [origin_actions.cpp](../../src/application/actions/origin_actions.cpp) | `moveOrigin(ctx, OriginTarget)` and `canMoveOrigin`: the one-click origin commands (geometry, bottom, world, world rotation on the selected origin's object; selection on the active object's selected vertices), each one undo step. `toggleOrigins` (View menu; hiding drops a selected origin). `beginOriginEdit` / `updateOriginEdit`: grab and rotate on an origin keep its starting transform and mesh in `ctx.originEdit` and set each frame's transform from them with `setOrigin`. |
| [light_markers.cpp](../../src/application/viewport/light_markers.cpp) | `drawLightMarkers`: projects each light to the screen and draws it with the UI draw list, so markers are a fixed pixel size and always on top of the scene. Orb in the light's color with a dark outline and soft glow (disabled: hollow gray ring); spot lights add an arrow along their direction and directional lights three parallel arrows (the outer two shorter). Arrows are built in world space and projected, so they foreshorten in 3D; their lengths are given in pixels and converted to world units at the light's distance so they stay a steady size on screen. Arrowheads have four fins (an X when seen end-on), and the side arrows are spread across the view; a faint line drops to the grid with a dot where it lands. Selected lights get a purple ring right against the orb's dark outline, with a soft purple glow behind it (the orb keeps its color). Farther lights are drawn first. Lights behind the camera are skipped. Sizes and colors are constants at the top of the file. |
| [object_commands.cpp](../../src/application/commands/object_commands.cpp) | `runObjectCommand`: everything behind the `object` command. `objectPresets()`, `addObject`, and `removeObject` are shared with the Objects tab; `addObject(ctx, Object)` places an object built elsewhere the same way, and `placeNewObject` does so without its own undo step (import adds a family as one). `setObjectParent(ctx, child, parent)`: a new parent as one undo step, printing what happened (drag and drop, the console). |
| [object_meshes.cpp](../../src/application/viewport/object_meshes.cpp) | `ObjectMeshCache`: one `OpenGLMesh` per object slot. `sync(handle, object, groupOf, materialsStamp)` creates it for a new object (or a new object in a reused slot); when `meshDirty`, or when the mesh's `MeshStamp` or the grouping stamp no longer matches the uploaded copy (`OpenGLMesh::matches`; so a change that didn't set the flag, like shading picked in the panel, still shows), it first tries `OpenGLMesh::patch` (vertices only moved) and rebuilds only if that can't be done, then clears the flag and the mesh's moved list; `prune` frees meshes of removed objects. Called from `renderFrame`. |
| [reference_images.cpp](../../src/application/viewport/reference_images.cpp) | Reference images in the app: `loadReferencePicture` (reads a PNG, at most 256 MB, and decodes it once to check it and get its size), `addReferenceImage` (2 units tall at the camera's target, turned with `rotationFacingView`, selected; one undo step), `addReferenceFrom(ctx, path)` (both, with a console message), `chooseReferenceImages` (the PNG dialog, starting in `ctx.referenceFolder`), `deleteSelectedReferences`, `drawReferenceImages(ctx, viewProjection, depth)` (one depth group, farthest first), and `drawReferenceOutlines` (a purple outline over a dark halo around each selected image, in the UI draw list). `ReferenceTextureCache` (`ctx.referenceTextures`): `sync` decodes and uploads a picture the first time it's drawn (a failure is reported once and the image is skipped), `prune` frees textures whose picture nothing holds any more, the scene or the undo history. |
| [material_view.cpp](../../src/application/viewport/material_view.cpp) | `surfaceFor(ctx, object)`: the `SurfaceLook` an object's faces draw with: its material's values in material view (`ctx.viewport.showMaterials`), with back faces culled unless the material is double-sided; `claySurface()` (the Default gray, back faces tinted) in clay view. Empty when nothing would show (blended at opacity 0, or a cutout below its cutoff, since without textures a cutout is all or nothing); blend at full opacity draws as opaque, the same picture for less work. `backFacesCulled(ctx, handle)`: material view with a single-sided material, used so clicks skip the back faces that aren't drawn. `worldCenter`: the middle of an object's bounding box in the world, for sorting see-through objects. `surfaceOf(material)`: a material's own look whatever the view. `partsFor(ctx, object, mesh)`: the object's `ObjectParts`, its mesh's face groups as `DrawPart`s split into solid and see-through (a group without its own material uses the object's; a material that shows nothing is left out), or `whole` in clay view. `faceGroupsFor(ctx)`: the `FaceGroupOf` uploads use (a face's material while it exists, otherwise the object's). `MaterialPreviewCache` (`ctx.materialPreviews`): one swatch texture per material (128 px), rendered by `sync` at the start of `renderFrame` (before the main pass) when the material is new or its look changed, and freed when it's removed; `texture(handle)` for the UI. |
| [shading_commands.cpp](../../src/application/commands/shading_commands.cpp) | `runShadingCommand` (the `shading` command); `shadingTargets` (the selected objects in object mode, otherwise the active object; none while a tool runs), `canSetShading` / `setShading` (one undo step for all targets), `canMarkEdges` (edge mode with edges selected) / `markSelectedEdges` (one undo step); `shadingName`. Used by the Edit ▸ Shading radial menu and the Objects tab. |
| [material_commands.cpp](../../src/application/commands/material_commands.cpp) | `runMaterialCommand` (the `material` command); `assignsToFaces` (face mode with faces selected), `materialTargets` (the selected objects in object mode, otherwise the active object), `assignMaterial` (objects), `assignFaceMaterial` (the selected faces, or `INVALID_MATERIAL` to clear), `selectFacesWithMaterial`, and `removeMaterial` (which marks every mesh dirty, since its faces regroup), each one undo step; `materialUse` (objects using it as theirs and faces with it as their own) and `describeUse`; `facesWithOwnMaterial`. |
| [stats_overlay.cpp](../../src/application/ui/stats_overlay.cpp) | `FrameStats` (`ctx.frameStats`: CPU input and render time, the last click's pick time, measured in `run`, `renderFrame` (until before `present`), and `checkSelectionContext`) and `drawStatsOverlay`: the stats readout in the top left (`stats` command, `ctx.viewport.showStats`, not saved), with the renderer's `getStats` and the scene's counts. |
| [reference_commands.cpp](../../src/application/commands/reference_commands.cpp) | `runReferenceCommand`: everything behind the `reference` command. |
| [light_commands.cpp](../../src/application/commands/light_commands.cpp) | `runLightCommand`: everything behind the `light` command. `deleteSelectedLights`: removes the selected lights as one undo step (used by the panel's − button and the Delete key) |
| [command_parsing.hpp](../../src/application/commands/command_parsing.hpp) | Argument parsers shared by commands: `parseFloat`, `parseUnitFloat` (0..1), `parseU32`, `parseVec3Args` |
| [application_scene.cpp](../../src/application/application_scene.cpp) | `initializeCamera`, `loadTestScene` |
| `action_checks/` | Per-context input handlers: the tools |
| [width_tool.hpp](../../src/application/tools/width_tool.hpp) | State kept while a bevel is in progress |

## Startup

`Application::initialize(ctx)`:
1. `createMainWindow` — 1920×1080 Win32 window.
2. `setupRenderer` — creates the OpenGL renderer with VSync on.
3. `registerInputEvents`, `registerDefaultActions`, `registerCommands`.
4. `initializeCamera` — camera at (0, 0, 3) looking at the origin, Y up.
5. `loadTestScene` — adds one object, `Cube`, made active, and the `Sun` directional light (its GPU mesh is built on the first frame).
6. Starts in vertex selection mode.
7. `initializeProject` — remembers the startup view (what Ctrl+N goes back to), marks the scene saved, and sets the title to `Untitled - Valuma Studio`.

## Main loop

`Application::run` loops `input.beginFrame()` → `Platform::pollEvents()` → `checkActions()` → `updateWindowTitle()` → `renderFrame()` until `is_running` is false. See [architecture.md](../architecture.md#frame-loop). Closing the window (or Alt+F4) triggers `Event::Quit`; its handler asks to save unsaved changes first (once, while the prompt is up), and Cancel keeps the app running.

`renderFrame` renders changed material swatches, draws reference images set to Behind, then a `DrawCommand` per object (with the selection as highlights, `surfaceFor` as its look, its solid parts from `partsFor`, and `outlineAll` for a selected object in object mode). Each object's see-through parts wait as their own command (without wireframe), together with reference images set to Scene; once everything solid is drawn they're sorted by distance from the camera (an object's `worldCenter`, an image's position) and drawn farthest first, so glass and pictures layer correctly. Then a `DrawGridCommand`, images set to Front, the debug overlay if on, then the UI. An object whose material shows nothing still draws its wireframe and selection. Vertex points are only shown in vertex mode.

## Modal windows

One modal window can be open at a time (`ctx.modal.kind`: `Prompt` or `Export`); opening one replaces the other. While open, `InputContext_Modal` is on, so only Enter (`ModalConfirm`) and Escape (`ModalCancel`) work as actions, and `checkActions` does nothing else: it dispatches those two and calls `updateModal`, then returns. `renderFrame` draws it with `drawModal` after the panel, inside the UI's draw (see [ui.md](ui.md#modal-windows)).

Clicks only record what was asked for (`prompt.chosen`, the Export window's `exportRequested`, `browseRequested`, `closeRequested`), and `updateModal` acts on it at the start of the next frame, so nothing changes the scene or opens a native dialog in the middle of drawing.

- **Prompt** (`showPrompt(ctx, title, message, buttons, confirmButton, cancelButton, onChoice)`): the message and a row of buttons on the right, the default one (Enter) outlined in purple. The prompt closes before `onChoice` runs, so the answer can open another window or a native dialog.
- **Export window:** see [vlmobj.md](vlmobj.md#in-the-app).

Import… in the Objects tab sets `ctx.importRequested`, and + in the Images tab `ctx.referenceRequested`; `checkActions` opens the file dialog on the next frame for the same reason.

## Action checks

`Application::checkActions` ([check_actions.cpp](../../src/application/actions/checks/check_actions.cpp)) runs every frame and dispatches by active context:

| Context active | Handler | File |
|---|---|---|
| always | Quit | `check_actions.cpp` |
| a modal window is open | `ActionMap::dispatch` (only Enter and Escape can fire) and `updateModal`; nothing below runs | `modal_windows.cpp` |
| `importRequested` | `importAssets` (the file dialog for Import…) | `asset_actions.cpp` |
| `referenceRequested` | `chooseReferenceImages` (the file dialog for + in the Images tab) | `reference_images.cpp` |
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

Markers are picked first, in every mode: an origin (`pickOrigin`, when origins show), then a light (`pickLight`), each by its distance on screen. Picking an origin selects just it and makes its object active (`Selection::selectOrigin`); Shift+click on the selected origin drops it. Picking a light clears selected elements and objects. Then a reference image (`pickReference`): it's taken unless a mesh under the mouse (`pickFace`, any object) hides it, which happens when the image is set to Behind or the mesh is nearer; an image set to Front always wins. Picking one selects it on its own; Shift+click toggles it. Otherwise, in object mode a click picks the nearest object with `pickFace` (any object), selecting it and making it active; Shift+click toggles it.

Mode switches (M+V / M+E / M+F / M+O and Tab, which clear the selection), starting tools, one-shot operators, and undo/redo run afterwards through `ActionMap::dispatch`.

Helpers in the file's anonymous namespace keep vertex selection in sync: `selectEdge` and `selectFace` also select their vertices; `deselectEdge` and `deselectFace` drop vertices no other selected element uses. Edges are treated as selected if either half-edge is.

## Modal tools

All follow the pattern in [architecture.md](../architecture.md#modal-tool-pattern): save start positions → `history.begin` → `setContext(tool)` → update each frame → confirm (`commit`) or cancel (restore + `cancel`) → `setContext(getSelectionContext())`.

In object mode the same three tools work on whole objects, in the world: `startGrab`/`startScale`/`startRotate` also save the selected objects' world transforms (`setObjectStartTransforms`). Grab adds the mouse movement to each world position (`setWorldPosition`); scale moves positions away from the objects' shared center and multiplies their world scale (kept at least 0.001 from zero; an axis lock limits both to the locked components), stored relative to the parent's; rotate turns positions around the shared center and composes the turn into each world rotation with `rotateEuler`, then `setWorldTransform`. A selected object whose parent (or grandparent…) is also selected is skipped (`carriedByParent`), since it comes along anyway. Cancel restores the saved transforms. Undo and redo drop selection that belongs to the other kind of mode (objects in edit modes, elements in object mode).

Vertex positions are in the active object's mesh space, but grab, scale, and rotate do their math in world space through `ObjectSpace` (see [scene.md](scene.md#transform)): grab converts the world movement with `directionToLocal`, and scale and rotate convert start positions to world, transform them, and convert back. So on a rotated, moved, or scaled object the selection follows the mouse and axis locks are world axes.

Start positions are stored on the selection with `Selection::setSelectionStartPositions`, in the same order as `getVertices()`.

### Grab — `checkGrabContext`
- Moves selected vertices, lights, objects, reference images, or a selected origin by mouse delta along the camera's right and up vectors. Speed is `0.001 × camera.distance` per pixel. G starts it when any of these are selected; their start positions are saved then (`setSelectionStartPositions`, `setLightStartPositions`, `setObjectStartTransforms`, `setReferenceStartTransforms`, `beginOriginEdit`).
- An origin moves with `updateOriginEdit`, which shifts the mesh back so it stays put (see [Origins](scene.md#origins)).
- With an axis lock on, movement is zeroed on the other axes. When a lock is first turned on, everything snaps back onto that axis through its start position.
- Cancel puts everything back at its start (the undo snapshot restores the rest).

### Scale — `checkScaleContext`
- Needs 2+ selected vertices, or selected objects or reference images. Reference images scale evenly from their shared center whatever the axis lock, keeping their proportions (size at least `MIN_REFERENCE_SIZE`).
- Scale factor = (mouse distance from the pivot on screen) ÷ (that distance when the tool started). The pivot is the average of the start positions, projected once on the first update (`initTransformTool`).
- Each frame rebuilds positions from the start positions: `center + (start − center) × factor`. With an axis lock, only the locked components scale; the others keep their start values.

### Rotate — `checkRotateContext`
- Needs 2+ selected vertices, or any selected lights, objects, or reference images, or a selected origin. Reference images orbit their shared center and turn with `rotateEuler`, like objects. Light directions are saved when R is pressed (`setLightStartDirections`). A selected origin turns its axes in place with `rotateEuler`, the mesh staying put; its position is the pivot.
- The angle follows the mouse around the pivot on screen: each frame adds the change in the mouse's angle around it (wrapped to ±π with `wrapAngle`), so circling more than once keeps turning. Counterclockwise on screen turns the selection counterclockwise as you see it.
- Each frame rebuilds from the start positions and start light directions. Vertices turn around the selection center; lights turn in place (only their direction rotates, renormalized; their position stays). With only lights selected, the pivot is their average position.
- Axis: camera forward by default, or world X/Y/Z when locked, flipped to point toward the viewer so the turn matches the mouse. Cancel restores start positions and light directions.

### Bevel — `beginBevel`, `checkBevelContext`
`ctx.widthTool` ([width_tool.hpp](../../src/application/tools/width_tool.hpp)) holds the state for bevel and inset.
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
