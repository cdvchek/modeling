# Features

The roadmap comes first: what's being worked on, what still has to be fixed, and ideas for later. After that is what the app does today.

When something ships, check it off and add it to [Current](#current). Delete checked items now and then so this list stays short.

## Planned

Work that's decided on. Rough order, top first; each step builds on the ones before it.

### UI, light markers, and objects

1. **Floating panel** (moving, resizing, and staying inside the window are done):
   - Click tabs to switch; mouse wheel scrolls content that doesn't fit.
   - Later: reorder tabs by dragging them within the tab bar. Tearing tabs off into separate panels is out of scope for now.
2. **Lights tab**: no refactor needed, all data and edit paths exist.
   - Ambient light and headlight sections at the top.
   - Light list; clicking a row selects the light, kept in sync with clicking markers in the viewport.
   - Add point / directional / spot, delete.
   - Selected light's properties: on/off, type, color, intensity, range, cone, position, direction.
3. **Multi-object support**: prerequisite for the Objects tab.
   - `ObjectCollection` stores a `DynamicArray<Object>` with `ObjectHandle`, like lights.
   - `Selection` stores object handles instead of indices, plus an active object.
   - Tools act on the active object instead of `objects.get(0)`.
   - History snapshots objects by handle, so adding and removing objects is undoable.
4. **Objects tab**:
   - Object list; clicking a row selects the object.
   - Add any preset (one button each), delete, show/hide, duplicate.
   - Transform fields: position, rotation, scale.

Code layout: `src/ui/` (draw list, UI context, widgets, panel; depends only on core math and input), `src/renderer/opengl/` (`drawUI`, UI and light-marker shaders), `src/application/` (tab contents, since they need `AppContext`).

### Gaps to close
Known limitations of what exists today.
- Tools only act on object 0 (`ctx.scene.objects.get(0)` is hard-coded in the tool code), though picking and selection support multiple objects. Fixed by planned step 3.
- Only one object exists, created at startup (a cube). No way to add, delete, or transform objects from the app. Fixed by planned steps 3 and 4.
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
- Spot light cone outline drawn while a spot light is selected.

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

### Light markers
- Every light is drawn in the viewport as a fixed-size orb in its color with a soft glow, always on top of the scene. Disabled lights are hollow gray rings. Spot lights show an arrow for where they point and directional lights three parallel arrows; every light has a faint line down to the grid.
- Selection is drawn in warm amber with a soft glow everywhere: selected vertices as amber discs with a dark outline and glow, edges as amber lines with a soft glow, faces amber-tinted (still lit) with an amber outline. Vertices are drawn as round dots.
- Grab (G) moves selected lights with X/Y/Z axis lock, cancel (right click), and undo.
- Rotate (R) aims selected lights: their direction turns around the view axis or a locked X/Y/Z axis while they stay in place. Cancel and undo work as for vertices.
- Lights and mesh elements are never selected at the same time: clicking (or Shift+clicking) one kind clears the other.
- Click a light's marker to select it (amber ring with a soft glow); Shift+click adds or removes. Lights are picked before mesh elements and can be selected in any mode.

### UI drawing
- Immediate-mode widgets (`ctx.ui`): button, list row, checkbox, slider, X/Y/Z drag fields, color swatch with RGB sliders, headings, separators. Clicks and scrolling over UI don't reach the viewport; a whole slider drag is one undo step. A floating panel (top right by default, `ui panel` toggles it) can be dragged by its header, resized from any edge or corner (with resize cursors and a minimum size), and always stays inside the viewport; it currently holds light controls. See [systems/ui.md](systems/ui.md#widgets-uicontext).
- Status bar along the bottom of the viewport showing frames per second (averaged over half a second), the selection mode (Vertex, Edge, Face), active tool (Select, Grab, Scale, Rotate, Bevel), and axis lock (`-` outside grab/scale/rotate, `Free`, or the locked axes in red/green/blue), separated by dividers. Items keep fixed positions as values change.
- 2D draw list (`ctx.uiDrawList`) for everything drawn over the viewport: rectangles, rounded rectangles with borders, soft shadows, anti-aliased lines, text, and nested clipping, drawn in a few batched calls. See [systems/ui.md](systems/ui.md).
- Two embedded fonts: the 16x24 console font and a 10x16 UI font (the console font trimmed and scaled down).

### Console
- Tab toggles an in-app console with command history (Up/Down) and cursor movement. Drawn with the UI draw list.
- Commands: `debug`, `validate`, `merge`, `dissolve`, `light`, `headlight`, `backface`, `vsync`, `ui`, `test`. See [systems/console.md](systems/console.md).

### Mesh
- Half-edge polygon mesh with generational handles.
- N-gon faces, triangulated with ear clipping for rendering (cached per face).
- Topology validator (`validate` command) that reports the first broken invariant.
- Built-in presets: cube, plane, grid, circle, cylinder, cone, UV sphere, ico sphere, torus (see [systems/mesh.md](systems/mesh.md#presets)). The startup object is the cube; change `PresetMesh::Cube` in `loadTestScene` to start with another.

### Platform and build
- Native Win32 window and input, OpenGL 3.3 via GLAD. No third-party frameworks.
- Console font embedded into the executable with `#embed`.
- CMake build: `modeling` executable, `modeling_core` static library, and a `tests` executable.
