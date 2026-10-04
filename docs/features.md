# Features

The roadmap comes first: what's being worked on, what still has to be fixed, and ideas for later. After that is what the app does today.

When something ships, check it off and add it to [Current](#current). Delete checked items now and then so this list stays short.

## Planned

Work that's decided on. Rough order, top first; each step builds on the ones before it.

### UI, light markers, and objects

1. **2D drawing layer**: UI code fills a backend-neutral `UIDrawList` (rects, lines, text in pixel coordinates, clip rects) and the renderer draws it with one `drawUI` call, so UI logic never touches OpenGL and can be unit tested. A rounded-rect shader with a soft drop shadow and 1 px border gives panels their floating look; `glScissor` clips panel contents. Needs a second, smaller bitmap font for UI text (the 16x24 console font is too big and doesn't scale cleanly).
2. **Status line**: a bar along the bottom showing mode, active tool, axis lock, selection counts, object and light counts, and frame time. First real use of the 2D layer.
3. **Light markers, selection, and moving lights**:
   - Markers: camera-facing billboards at a fixed pixel size, drawn as a disc with a soft glow in the light's color. Directional lights add a short direction line (their marker sits at `light.position`), spot lights show their cone outline while selected, and a faint line drops to the grid. Selected = yellow ring; disabled = hollow gray ring.
   - Picking: screen-space distance to the marker, checked before mesh elements.
   - Selection: `Selection` can hold a light. Grab (G) moves it with axis lock and undo.
4. **UI core**:
   - Input routing: the UI sees the mouse first each frame and reports whether it used it; viewport picking and camera orbit only run when it didn't.
   - Immediate-mode widgets with IDs and hot/active tracking: label, button, selectable list row, checkbox, drag slider, color swatch with RGB sliders.
   - Slider drags call `history.begin` on press and `commit` on release, so one drag is one undo step.
   - No text input yet; renaming stays a console command.
5. **Floating panel**: one panel with tabs, state kept between frames (position, size, tabs, active tab, scroll).
   - Drag by the tab bar or header; kept inside the window.
   - Resize from edges and corners with a minimum size; resize cursor on hover (Win32 `SetCursor`).
   - Click tabs to switch; mouse wheel scrolls content that doesn't fit.
   - Later: reorder tabs by dragging them within the tab bar. Tearing tabs off into separate panels is out of scope for now.
6. **Lights tab**: no refactor needed, all data and edit paths exist.
   - Ambient light and headlight sections at the top.
   - Light list; clicking a row selects the light, kept in sync with clicking markers in the viewport.
   - Add point / directional / spot, delete.
   - Selected light's properties: on/off, type, color, intensity, range, cone, position, direction.
7. **Multi-object support**: prerequisite for the Objects tab.
   - `ObjectCollection` stores a `DynamicArray<Object>` with `ObjectHandle`, like lights.
   - `Selection` stores object handles instead of indices, plus an active object.
   - Tools act on the active object instead of `objects.get(0)`.
   - History snapshots objects by handle, so adding and removing objects is undoable.
8. **Objects tab**:
   - Object list; clicking a row selects the object.
   - Add any preset (one button each), delete, show/hide, duplicate.
   - Transform fields: position, rotation, scale.

Code layout: `src/ui/` (draw list, UI context, widgets, panel; depends only on core math and input), `src/renderer/opengl/` (`drawUI`, UI and light-marker shaders), `src/application/` (tab contents, since they need `AppContext`).

### Gaps to close
Known limitations of what exists today.
- Tools only act on object 0 (`ctx.scene.objects.get(0)` is hard-coded in the tool code), though picking and selection support multiple objects. Fixed by planned step 7.
- Only one object exists, created at startup (a cube). No way to add, delete, or transform objects from the app. Fixed by planned steps 7 and 8.
- Extrude and inset only use the first selected face.
- Bevel refuses geometry that touches a mesh border.
- No save/load or import/export.
- Unknown console commands are silently ignored, and there is no `help` command.
- `Object` holds an `OpenGLMesh` directly, so the scene depends on the OpenGL backend.
- Windows-only; only the OpenGL backend exists.

## Ideas

Not committed to yet. Move an item into [Planned](#planned) when you decide to do it.

### Good next picks
Small, builds on code that already exists, and very useful day to day:
- **Loop cut**: split every face along an edge ring (ring selection already exists).
- **Preset parameters from the app**: the Objects tab will add presets at their defaults; exposing `MeshFactory` parameters (sides, segments, radius) as panel fields or an `add cylinder 16` command.
- **OBJ export/import**: simple text format that handles n-gons, so models can leave the app.
- **Numeric input while transforming**: type `2` during grab/scale/rotate for exact values.
- **Select all / none / invert, select linked**.
- **`help` command** listing commands and their descriptions.

### Modeling tools
- Region extrude: extrude several connected faces as one piece; extrude edges and vertices.
- Knife: cut new edges across faces by clicking points.
- Subdivide: split faces into quads; later a Catmull-Clark subdivision preview.
- Bridge two edge loops.
- Edge slide and vertex slide.
- Merge by distance (weld nearby vertices).
- Flip normals / make normals consistent.
- Triangulate faces, and tris-to-quads.
- Poke face: add a center vertex and fan out triangles.
- Spin / screw: sweep a profile around an axis.
- Mirror editing: edits on one side are repeated on the other.
- Proportional editing: moving a vertex drags nearby vertices with a falloff.

### Transform and snapping
- Grid snapping and vertex snapping during grab.
- Pivot options: selection center, individual element origins, a placeable 3D cursor.
- Local axis lock (along a face normal) as well as world X/Y/Z.

### Selection
- Box select and lasso select.
- Grow / shrink selection.
- Hover highlight of the element under the mouse.
- X-ray toggle so you can select through the mesh.

### Objects and scene
- Object mode vs. edit mode: move, rotate, and scale whole objects.
- Duplicate, join, and separate objects.

### Viewport
- Orthographic front/side/top views on hotkeys, and a perspective/ortho toggle.
- Frame selected: move the camera to fit the selection.
- Small axis gizmo in a corner showing the view orientation.
- Display toggles: face normals, backface culling, wireframe-only, x-ray.
- Vertex/edge/face counts for the whole mesh in the status line.

### Workflow and engineering
- Native save format plus autosave.
- Store undo as diffs instead of full snapshots to cut memory use.
- Configurable key bindings.
- Tests for each tool (extrude, inset, bevel, etc.) that run `validate` afterward.

## Current

### Viewport and camera
- Orbit camera around a target point (right mouse drag). Pitch is clamped just short of straight up/down.
- Pan the camera and its target together (middle mouse drag). Pan speed scales with zoom distance.
- Zoom with the mouse wheel. Minimum distance is 0.5.
- Y-up world. 60° field of view, near plane 0.1, far plane 1000.
- Resizable window; the frame keeps drawing while the window is being resized.

### Scene display
- Faces drawn in gray, flat-shaded per triangle by the scene's ambient, directional, point, and spot lights (managed with the `light` console command); the default scene has one sun. A camera headlight (on by default, `headlight` command) lights whatever you're looking at. Back faces are tinted red (`backface tint r g b` to change). Edges in dark gray, vertices as near-black points (vertices only shown in vertex mode).
- 4× MSAA anti-aliasing on everything drawn (edges, points, face outlines).
- Vertical gradient background, lighter at the top, dithered to avoid banding.
- Selected elements drawn in yellow.
- Infinite ground grid on the XZ plane:
  - Spacing changes with zoom: 0.5 → 2.5 → 12.5 → 62.5 (each level 5× the last).
  - Every 5th line is thicker. Levels cross-fade smoothly as you zoom.
  - Anti-aliased lines with a fixed pixel width; dense lines fade out to avoid moiré.
  - Fades out with distance instead of ending at a hard edge.
  - X axis drawn in red, Z axis in blue.
- Debug overlay (console command `debug`): draws every half-edge as an arrow with its `index:generation` label.

### Selection
- Three selection modes: vertex, edge, face. Switching modes clears the selection.
- Click to select; Shift+click to add/remove.
- Ctrl+click: select an edge loop (edge mode) or face loop (face mode).
- Alt+click: select an edge ring (edge mode).
- Picking is ray-based against vertices, edges, and triangles; the closest hit wins.
- Selecting an edge or face also selects its vertices.

### Editing tools
| Tool | Mode | What it does |
|---|---|---|
| Grab | any | Move selected vertices with the mouse. Optional X/Y/Z axis lock. |
| Scale | any (2+ verts) | Scale selected vertices around their center, driven by mouse distance from screen center. Optional axis lock. |
| Rotate | any (2+ verts) | Rotate selected vertices around their center. Rotates around the view axis, or a locked X/Y/Z axis. |
| Extrude | face | Extrudes the first selected face, then starts a grab. |
| Inset | face | Inserts a face ring on the first selected face, then starts a scale. |
| Bevel | any (exactly 1 element) | Bevels one vertex, edge, or face; width follows the mouse. Interior geometry only. |
| Delete | any | Removes the selected vertices, edges, or faces. |
| Connect | vertex (2 verts) | Adds an edge between two vertices that share a face, splitting the face. |
| Fill | edge | Fills the open border loop that the first selected edge lies on with a new face. |
| Merge | vertex (2 connected verts) | Console command `merge [center\|first\|last]`. Collapses the edge between them. |
| Dissolve | edge or face (1 element) | Console command `dissolve`. Collapses an edge to its midpoint or a face to its center. |

All edits can be confirmed (left click) or cancelled (right click) while active.

### Undo / redo
- Ctrl+Z / Ctrl+Y. Up to 100 steps.
- Snapshots mesh data, object transforms, lights, and selection.

### Console
- Tab toggles an in-app console with command history (Up/Down) and cursor movement.
- Commands: `debug`, `validate`, `merge`, `dissolve`, `light`, `headlight`, `backface`, `test`. See [systems/console.md](systems/console.md).

### Mesh
- Half-edge polygon mesh with generational handles.
- N-gon faces, triangulated with ear clipping for rendering (cached per face).
- Topology validator (`validate` command) that reports the first broken invariant.
- Built-in presets: cube, plane, grid, circle, cylinder, cone, UV sphere, ico sphere, torus (see [systems/mesh.md](systems/mesh.md#presets)). The startup object is the cube; change `PresetMesh::Cube` in `loadTestScene` to start with another.

### Platform and build
- Native Win32 window and input, OpenGL 3.3 via GLAD. No third-party frameworks.
- Console font embedded into the executable with `#embed`.
- CMake build: `modeling` executable, `modeling_core` static library, and a `tests` executable.
