# Features

The roadmap comes first: what's being worked on, known gaps, and ideas for later. After that is everything the app does today.

When something ships, move it from [Planned](#planned) to [Current](#current).

## Planned

Work that's decided on. Each area is broken into steps in the order they'd be built. Status: **Next** is being worked on (or up next), **Later** is decided but not started, **Done** steps stay listed until the whole area ships.

| Area | Status | Depends on |
|---|---|---|
| [Import/export](#importexport) | **Next** | — |
| [Reference images](#reference-images) | Later | — |
| [Materials](#materials) | Later | — |
| [Textures](#textures) | Later | Materials (a texture is something a material uses) |
| [Animation](#animation) | Later | Import (to animate models you're given) |

### Import/export
Get models in and out of the app, so they can be used in a game and so models made elsewhere can be edited (and later animated) here.
1. **Pick the format.** Still to decide. OBJ is simple and handles n-gons but only carries geometry (and basic materials). glTF also carries materials, textures, skeletons, and animations, which the later areas need, so it would grow with the app.
2. Export the scene's objects (mesh, transform, name).
3. Import into the scene as new objects, n-gons kept.
4. A native save format for the whole scene (objects, lights, settings), so work can be closed and reopened.

### Reference images
Put pictures in the scene to model against (a front and side view of a character, a photo of a prop).
1. Load an image file (PNG and JPEG at least) as a texture on the GPU.
2. A reference image object: a flat quad showing the image, placed in the scene.
3. Position, rotation, and scale it like an object (object-mode grab/scale/rotate and the panel), keeping the image's aspect ratio.
4. Display options: opacity, draw behind or over the mesh, lock it so clicks go through to the model, show/hide.
5. A Reference tab in the panel listing them, with + (pick a file) and −.

### Materials
Give surfaces a look beyond flat gray, and carry it into the game.
1. A material: name, base color, roughness, metallic (the usual game-engine set, so it exports cleanly).
2. Shading that uses them (replacing the single gray), still lit by the scene's lights.
3. Assign materials per object, then per face (face mode: select faces, pick a material).
4. A Materials tab in the panel: list, add, remove, edit; a swatch preview.
5. Included in export and the save format.

### Textures
Paint and apply images to surfaces.
1. **UV coordinates** on the mesh: per face corner, kept through every editing tool.
2. Unwrapping: simple projections first (planar, box, cylinder), then seams and an automatic unwrap.
3. A UV editor view to see and adjust UVs over the texture.
4. Materials use textures (base color first; later roughness, normal maps).
5. Texture painting: brush color, size, and softness, painted in the viewport and the UV view; saved as PNG.
6. Textures included in export.

### Animation
Animate models, including ones made elsewhere.
1. Skeletons: bones in a hierarchy, shown in the viewport; a pose mode to rotate and move bones.
2. Skinning: each vertex weighted to bones (automatic weights first, then weight painting).
3. Keyframes on bones (and object transforms): a timeline with play, scrub, and keys.
4. Interpolation (linear, smooth) and several named animations (clips) per model.
5. Import rigs and animations from a model you're given; export them for the game.

## Gaps to close

Known limitations of what exists today.

- No save/load or import/export (see [Import/export](#importexport)).
- No tone mapping: strong lights on top of the default ambient, sun, and headlight clip to white quickly.
- Windows-only; only the OpenGL backend exists.

## Ideas

Not committed to yet. Move an item into [Planned](#planned) when you decide to do it.

### Good next picks
Small, builds on existing code, and useful day to day:
- **Loop cut**: split every face along an edge ring (ring selection already exists).
- **Numeric input while transforming**: type `2` during grab/scale/rotate for exact values.
- **Select all / none / invert, select linked**.
- **Preset parameters**: expose `MeshFactory` parameters (sides, segments, radius) in the Objects tab or as `object add cylinder 16`.

### Modeling tools
- Extrude edges and vertices.
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
- Duplicate, join, and separate objects.
- Show/hide objects.

### Viewport
- Orthographic front/side/top views on hotkeys, and a perspective/ortho toggle.
- Frame selected: move the camera to fit the selection.
- Small axis gizmo in a corner showing the view orientation.
- Display toggles: face normals, backface culling, wireframe-only, x-ray.
- Spot light cone outline while a spot light is selected.

### Interface
- Light type picker as a dropdown, matching the Objects tab.

### Workflow and engineering
- Autosave (once the native save format from [Import/export](#importexport) exists).
- Store undo as diffs instead of full snapshots to cut memory use.
- Configurable key bindings.

## Current

### Viewport and camera
- Orbit around a target point (right mouse drag). Pitch is clamped just short of straight up/down.
- Pan the camera and its target together (middle mouse drag). Pan speed scales with zoom distance.
- Zoom with the mouse wheel. Minimum distance is 0.5.
- Y-up world. 60° field of view, near plane 0.1, far plane 1000.
- Resizable window; the frame keeps drawing while the window is being resized.
- Mouse capture: a drag that ends outside the window still finishes cleanly.

### Rendering
- Faces are gray and flat-shaded per triangle, so a non-planar face shows its fold.
- Back faces are tinted pink so flipped or open faces stand out (`backface tint r g b` to change).
- Edges in dark gray; vertices as near-black round dots (vertices only shown in vertex mode).
- 4× MSAA anti-aliasing.
- Colors follow the Dracula theme: blue-grey backgrounds, purple selection and highlights, green headings and text input, red errors.
- Vertical gradient background in Dracula blue-greys, lighter at the top, dithered to avoid banding.
- Infinite ground grid on the XZ plane:
  - Spacing changes with zoom: 0.5 → 2.5 → 12.5 → 62.5 (each level 5× the last), cross-fading smoothly; every 5th line is thicker.
  - Anti-aliased lines with a fixed pixel width; dense lines fade out to avoid moiré.
  - Fades out with distance; X axis in red, Z axis in cyan.
- Debug overlay (`debug` command): every half-edge as an arrow with its `index:generation` label.
- `vsync on | off` (on by default).

### Lights
- Types: point, spot, and directional, each with color, intensity, and on/off. Point and spot lights have a position and range (smooth falloff to zero); spot lights have inner and outer cone angles; directional and spot lights have a direction. Up to 4 directional and 8 point/spot lights shade at once.
- Ambient light (color and strength) and a camera headlight (on by default; color and strength; a view setting, not part of undo).
- The default scene has one directional sun.
- Markers: every light is drawn as a fixed-size orb in its color with a soft glow, always on top of the scene, with a faint line down to the grid. Disabled lights are hollow gray rings. Spot lights show a 3D arrow for their direction; directional lights show three parallel arrows.
- Click a marker to select a light (purple ring); Shift+click adds or removes. Lights are picked before mesh elements, in any selection mode.
- Grab (G) moves selected lights; Rotate (R) turns their direction in place. Both support X/Y/Z axis lock, cancel, and undo. Delete removes selected lights.
- New lights are named Light 1, Light 2, … (one past the highest number in use, so names never repeat after deleting).
- `light` console command: list, add, remove, and edit every property (see [systems/console.md](systems/console.md#light-command)).

### Objects
- Several objects in one scene; one is the **active object** you're editing. Only it shows its wireframe, vertices, and selection; the others show just their faces.
- Managed from the panel's Objects tab or the `object` command.
- Click another object (in any edit mode) to make it the active object, staying in the same mode.
- **Object mode** (Tab toggles it with the last edit mode; also M+O or the radial Mode menu): work with whole objects.
  - Click an object to select it, Shift+click to add or remove. The last one clicked becomes the active object (the one Tab edits). Entering object mode selects the active object.
  - Selected objects are outlined in purple; the panel's object list highlights them.
  - Grab, scale, and rotate change the selected objects' transforms (position, rotation, scale), so the panel fields follow. With several objects, scale spreads them out from their shared center and rotate turns them around it. Axis locks work as in edit mode; locked scale changes only those scale components.
  - Delete removes the selected objects as one undo step.
- `object` console command: list, add any preset, remove, rename, move, rotate, scale, and switch the active object (see [systems/console.md](systems/console.md#object-command)). Adding, removing, and editing objects are undoable.

### Selection
- Three edit modes for mesh elements, vertex, edge, and face (M+V, M+E, M+F), plus object mode for whole objects (M+O; Tab switches between object mode and the last edit mode). Switching modes clears the selection.
- Click to select elements of the active object; Shift+click to add or remove.
- Ctrl+click: edge loop (edge mode) or face loop (face mode). Alt+click: edge ring (edge mode).
- Picking is ray-based against vertices, edges, and triangles; the closest hit wins.
- Selecting an edge or face also selects its vertices.
- Lights and mesh elements are never selected together: clicking one kind clears the other.
- Selection is drawn in purple with a soft glow: vertices as purple dots with a dark outline, edges as purple lines, faces purple-tinted (still lit) with an purple outline, lights with an purple ring.

### Editing tools
| Tool | Works on | What it does |
|---|---|---|
| Grab (G) | Vertices, lights, or objects | Moves them with the mouse. Optional X/Y/Z axis lock. |
| Scale (S) | 2+ vertices, or objects | Scales around their center by how far the mouse is from that center compared with where it started. A guide line runs from the center to the mouse. Optional axis lock. |
| Rotate (R) | 2+ vertices, lights, or objects | Circle the mouse around the selection's center to turn it with the mouse (full turns add up). A guide line runs from the center to the mouse. Rotates vertices around their center; turns lights' direction in place. View axis, or a locked X/Y/Z axis. |
| Extrude | 1+ faces | Region extrude: selected faces that share edges extrude as one block, with walls only around the outside; separate regions extrude together. Then starts a grab. |
| Inset | 1+ faces | Region inset: each region gets one border ring around its outside, every edge moved in by the same width (world space). Same control as bevel: a dot where the mouse started and a line to it; away widens, back narrows. |
| Bevel | 1 vertex, edge, or face | A dot marks where the mouse started, with a line to the mouse; moving away widens the bevel, moving back narrows it. Works on open meshes too: a corner with two open edges gets cut off, cuts reaching an open edge slide along it, and beveling an open edge itself adds a strip on its face side while the outline stays put. |
| Delete | Vertices, edges, faces, lights, or objects | Removes them. |
| Connect | 2 vertices on one face | Adds an edge between them, splitting the face. |
| Fill | 1 border edge | Fills the open border loop it lies on with a new face. |
| Merge | 2 connected vertices | Radial menu (at the center) or `merge [center\|first\|last]`. Collapses the edge between them. |
| Dissolve | 1 edge or face | Radial menu or `dissolve`. Collapses an edge to its midpoint or a face to its center. |

Extrude and inset refuse selections where faces touch only at a corner, a region with no boundary (a closed surface), and a region with a hole. Refusals (from these, bevel, and dissolve) show as an error in the console and briefly in the status bar.

The tools work in world space, so they behave the same on a moved, rotated, or scaled object as on an untransformed one: the selection follows the mouse, X/Y/Z locks are world axes, and bevel and inset widths match the mouse distance.

Modal tools are confirmed with left click and cancelled with right click.

### Radial menu
Hold the thumb side button (Mouse4) to open a radial menu at the cursor, move toward a slice, and release to run it. Releasing in the middle, or right clicking, closes it without doing anything. While it's open, the camera, tools, and panel hold still.

- A menu's items split the circle evenly, with no empty slots: the first item is centered straight up and the rest go clockwise. Even counts mirror left/right and up/down; odd counts mirror only left/right. Items that can't run right now are dimmed, never removed, so positions stay put. Each label shows its key shortcut faintly.
- Moving past the ring toward a submenu (`>`) opens it, re-centered at the cursor. Two levels at most.
- Drawn with ring slices in the UI shader; the hovered slice is purple.

| Menu | Items, clockwise from the top |
|---|---|
| Main (8) | Grab, Scale, Edit ▸ (Light ▸ with lights selected), View ▸, Mode ▸, Redo, Undo, Rotate |
| Edit ▸ (8) | Extrude, Inset, Fill, Dissolve, Delete, Merge, Connect, Bevel |
| Light ▸ (5) | Spot, Directional, On/Off, Delete, Point |
| Mode ▸ (4) | Edge, Face, Object, Vertex |
| View ▸ (3) | Hide/Show panel, Headlight off/on, Debug on/off (each label says what picking it will do) |
| While grab/scale/rotate/bevel/inset runs (4) | Y (up), Z (right), Free (down), X (left) |

Axis items toggle like the X/Y/Z keys; Free clears every lock. In the Light menu, a type is dimmed when every selected light already has it, and On/Off turns them all off if all are on, otherwise all on.

The menu replaces the awkward key chords (like M+V and M+F for modes). Everything else keeps its keyboard shortcut and isn't meant to move into the menu: Shift/Ctrl/Alt+click selection, Tab, the console, and confirm/cancel on the mouse buttons.

### Undo / redo
- Ctrl+Z / Ctrl+Y, up to 100 steps.
- Snapshots all objects, lights (including ambient), and the selection, so adding and removing objects and lights is undoable too.
- A whole drag of a panel slider or field is one undo step.

### Interface
- **Floating panel** (shown by default, `ui panel` toggles it): drag it by its header or tabs, resize it from any edge or corner (resize cursors, 240 × 160 minimum), and scroll it with the wheel or its scrollbar when the content doesn't fit. It always stays inside the viewport.
- **Objects tab** (first):
  - A preset dropdown (cube, plane, grid, circle, cylinder, cone, UV sphere, ico sphere, torus), + to add that preset, − to remove the object being edited.
  - A fixed-height object list that scrolls on its own; click a row to edit that object (synced with clicking it in the viewport). In object mode it shows and sets the selected objects.
  - The selected object: name field, vertex/edge/face counts, and position, rotation, and scale fields.
- **Lights tab:**
  - A type button (click to cycle point → spot → directional), + to add that type, − to delete the selected light (dimmed when nothing is selected).
  - A fixed-height light list that scrolls on its own; click a row to select the light (synced with the viewport).
  - The selected light: name field, type switch, enabled, color, intensity, and the position, range, direction, and cone fields that apply to its type.
  - Ambient light and headlight.
- Clicks and scrolling over the panel don't reach the viewport.
- **Text fields** (object and light names): click to edit with everything selected, type to replace it. Arrow keys, Home/End (Shift extends the selection), Backspace/Delete, Ctrl+A, and Ctrl+C/X/V with the system clipboard; click inside to place the caret or drag to select. Enter or a click anywhere else keeps the edit (one undo step), Escape restores the old text, and an empty name is ignored. While editing, keyboard shortcuts are off, so typing `g` or `/` just types.
- **X/Y/Z fields**: drag to change; click without dragging (under 3 px of movement) to type an exact value, with the same keys. Text that isn't a number leaves the value alone.
- Held keys repeat after the system's repeat delay, in text fields and in the console.
- **Status bar** along the bottom: frames per second, selection mode, active tool, and axis lock (`-` outside grab/scale/rotate, `Free`, or the locked axes in red/green/cyan). Items keep fixed positions as values change. An error that happens while the console is closed (an unknown command, a refused bevel/extrude/inset/dissolve) shows in red at the right end for 4 seconds, fading out at the end.
- **Console** (/): a panel docked above the status bar with commands, their output, and errors (red) above an input line with a blinking caret. `help` lists every command; an unknown command shows an error; tools report refusals there too (bevel, extrude, inset, dissolve). Output or errors longer than one line can be collapsed and expanded by clicking; closing the console collapses everything so far, so only new or reopened ones show expanded. The wheel scrolls the list; command history (Up/Down, with the recalled command highlighted in the list, which scrolls to keep it in view) and cursor movement; Backspace, Delete, and the arrow keys repeat while held. Commands: `help`, `debug`, `validate`, `merge`, `dissolve`, `light`, `object`, `headlight`, `backface`, `vsync`, `ui`. See [systems/console.md](systems/console.md).
- Built on a from-scratch immediate-mode UI (tabs, buttons, dropdowns, list rows, checkboxes, sliders, X/Y/Z fields, text fields, color swatches, type switches, scrolling list boxes) and a batched 2D draw list. See [systems/ui.md](systems/ui.md).
- Two embedded fonts: the 16×24 console font and a 10×16 UI font (the console font trimmed and scaled down).

### Mesh
- Half-edge polygon mesh with generational handles.
- N-gon faces, triangulated with ear clipping for rendering (cached per face).
- Topology validator (`validate` command) that reports the first broken invariant.
- Built-in presets: cube, plane, grid, circle, cylinder, cone, UV sphere, ico sphere, torus (see [systems/mesh.md](systems/mesh.md#presets)). The startup object is a cube; add more from the Objects tab or `object add <preset>`.

### Platform and build
- Native Win32 window, input, and clipboard; OpenGL 3.3 via GLAD. No other third-party code.
- Fonts and shaders are embedded into the executable with `#embed`.
- CMake build: `modeling` executable, `modeling_core` static library, and a `tests` executable.
