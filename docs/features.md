# Features

The roadmap comes first: what's being worked on, known gaps, and ideas for later. After that is everything the app does today.

When something ships, move it from [Planned](#planned) to [Current](#current).

## Planned

Work that's decided on. Each area is broken into steps in the order they'd be built. Status: **Next** is being worked on (or up next), **Later** is decided but not started, **Done** steps stay listed until the whole area ships.

| Area | Status | Depends on |
|---|---|---|
| [Textures](#textures) | Next | — (materials are done; reading PNGs and GPU textures already exist, from reference images; UVs are done) |
| [Animation](#animation) | Later | — |
| [Attachment points](#attachment-points) | Later | Animation for points that follow bones |
| [Hitboxes and collision shapes](#hitboxes-and-collision-shapes) | Later | Animation for shapes that follow bones |

### Textures
Paint and apply images to surfaces. Reading PNG files (`shared/image/`) and uploading them as GPU textures with mipmaps already work, from [reference images](#reference-images).
1. **UV coordinates** on the mesh: per face corner, kept through every editing tool. **Done**: every preset has UVs, the UV grid shows them, saved and exported.
2. Unwrapping: simple projections first (planar, box, cylinder), then seams and an automatic unwrap.
3. A UV editor view to see and adjust UVs over the texture.
4. Materials use textures (base color first; later roughness, normal maps).
5. Texture painting: brush color, size, and softness, painted in the viewport and the UV view; saved as PNG (needs a PNG writer in `shared/image/`).
6. Textures included in export.

### Attachment points
Named spots on an asset (`hand_R`, `mouth`, `head`) where the game attaches weapons and effects or places cameras, so the asset tells the game where without containing code.
1. A point is a name and a transform on an object (or a bone, with Animation).
2. Drawn as small axes markers; moved with grab and rotate; listed in the panel.
3. Exported with the asset.

### Hitboxes and collision shapes
Simple shapes for the engine's physics and hit detection, made where the asset is made: a monster's hitboxes ride on its bones, a rock gets a hull. Valuma only shapes, places, and names them; what they do (damage, headshot multipliers, mass, friction, ragdoll joints, collision layers, triggers) belongs to Aevora and the game code.
1. Shapes: box, sphere, capsule, convex hull, each with a name (`head`, `torso`, `left_arm`) and attached to an object, or to a bone once Animation exists.
2. Make them by hand, or generate one around the selection (such as a hull around a rock).
3. Shown as wireframes, with a toggle to hide them.
4. Exported with the asset in the reserved `COLL` section. For simple props, Aevora can also generate a hull itself on import, so hand-made shapes matter most for characters and props whose automatic shape isn't good enough.

### Animation
Animate models, including ones made elsewhere.
1. Skeletons: bones in a hierarchy, shown in the viewport; a pose mode to rotate and move bones.
2. Skinning: each vertex weighted to bones (automatic weights first, then weight painting).
3. Keyframes on bones (and object transforms): a timeline with play, scrub, and keys.
4. Interpolation (linear, smooth) and several named animations (clips) per model.
5. Events on a clip ("footstep at frame 12", "hit at frame 20") that the game reacts to.
6. Import rigs and animations from a model you're given; export them for the game.

## Gaps to close

Known limitations of what exists today.

- Assets only go in and out as `.vlmobj`; no glTF or OBJ yet (a separate tool later).
- Materials: without textures, Cutout can only show or hide a whole object (opacity is one value per material); it's ready for textures. See-through objects are sorted as whole objects, so two that pass through each other can blend in the wrong order in places. Material swatches use fixed studio lighting, not the scene's.
- Reference images: PNG only, and not interlaced PNGs; JPEG comes later. A new image always faces the current view; a choice of Front / Side / Top (so it lines up with the axes) comes later. Clicking a fully transparent part of an image still selects it.
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

### Assets
- Levels of detail: simpler versions of a mesh for far away, made in Valuma and carried in the asset (a section is reserved in the format).
- glTF or OBJ export, as a separate tool, for other programs.

### Workflow and engineering
- Faster clicking on big meshes: skip objects whose bounds the mouse ray misses, then a spatial structure inside each mesh so a click doesn't test every triangle (the stats readout shows the last pick's time).
- Store undo as diffs instead of full snapshots to cut memory use.
- Autosave and a recent-files list for projects.
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
- Faces are drawn in their object's material (see [Materials](#materials); the Default gray otherwise) and flat-shaded per triangle, so a non-planar face shows its fold.
- In clay view, back faces are tinted pink so flipped or open faces stand out (`backface tint r g b` to change); in material view they're culled unless the material is double-sided.
- Edges in dark gray; vertices as near-black round dots (vertices only shown in vertex mode).
- 4× MSAA anti-aliasing.
- Lighting is done in linear color: picked colors (sRGB, as color pickers and image editors show them) are converted before they're lit, and the result goes through tone mapping (Khronos PBR Neutral, which leaves ordinary colors alone and rolls bright ones off toward white instead of clipping) and back to sRGB for the screen. The game engine is meant to do the same, so assets look alike in both.
- **Exposure** (Lights tab, or `exposure [<stops>]`): brightens or darkens lit surfaces, each stop doubling or halving the light, from −5 to +5. A view setting saved with the project, not undoable.
- Reference images as textured planes, see-through by their own alpha and an opacity, drawn behind everything, among the meshes, or over everything (see [Reference images](#reference-images)).
- Colors follow the Dracula theme: blue-grey backgrounds, purple selection and highlights, green headings and text input, red errors.
- Vertical gradient background in Dracula blue-greys, lighter at the top, dithered to avoid banding.
- Infinite ground grid on the XZ plane:
  - Spacing changes with zoom: 0.5 → 2.5 → 12.5 → 62.5 (each level 5× the last), cross-fading smoothly; every 5th line is thicker.
  - Anti-aliased lines with a fixed pixel width; dense lines fade out to avoid moiré.
  - Fades out with distance; X axis in red, Z axis in cyan.
- Debug overlay (`debug` command): every half-edge as an arrow with its `index:generation` label.
- `vsync on | off` (on by default).
- **Stats** (`stats`): a readout in the top left with CPU time for input and for building the frame, GPU time, the last click's picking time, draw calls, triangles, lines, points, shader value uploads, mesh rebuilds and in-place updates with their sizes, and the scene's objects, faces, and vertices. Selections draw in one call each (a whole selected outline in object mode included), the lights go to the GPU once per frame in one buffer, and while grab, rotate, scale, bevel, inset, or an origin move changes positions only the moved vertices and the faces around them are sent again.

### Lights
- Types: point, spot, and directional, each with color, intensity, and on/off. Point and spot lights have a position and range (smooth falloff to zero); spot lights have inner and outer cone angles; directional and spot lights have a direction. Up to 4 directional and 8 point/spot lights shade at once.
- Ambient light (color and strength, 0.3 by default) and a camera headlight (on by default at 0.8; color and strength; a view setting, not part of undo).
- The default scene has one directional sun at intensity 1.2.
- Markers: every light is drawn as a fixed-size orb in its color with a soft glow, always on top of the scene, with a faint line down to the grid. Disabled lights are hollow gray rings. Spot lights show a 3D arrow for their direction; directional lights show three parallel arrows.
- Click a marker to select a light (purple ring); Shift+click adds or removes. Lights are picked before mesh elements (and after origins), in any selection mode.
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
- Lights, origins, reference images, and mesh elements are never selected together: clicking one kind clears the others.
- Selection is drawn in purple with a soft glow: vertices as purple dots with a dark outline, edges as purple lines, faces purple-tinted (still lit) with a purple outline, lights with a purple ring, a selected origin purple with its axes.

### Editing tools
| Tool | Works on | What it does |
|---|---|---|
| Grab (G) | Vertices, lights, objects, or reference images | Moves them with the mouse. Optional X/Y/Z axis lock. |
| Scale (S) | 2+ vertices, objects, or reference images | Scales around their center by how far the mouse is from that center compared with where it started. A guide line runs from the center to the mouse. Optional axis lock (not for reference images, which always scale evenly). |
| Rotate (R) | 2+ vertices, lights, objects, or reference images | Circle the mouse around the selection's center to turn it with the mouse (full turns add up). A guide line runs from the center to the mouse. Rotates vertices around their center; turns lights' direction in place. View axis, or a locked X/Y/Z axis. |
| Extrude | 1+ faces | Region extrude: selected faces that share edges extrude as one block, with walls only around the outside; separate regions extrude together. Then starts a grab. |
| Inset | 1+ faces | Region inset: each region gets one border ring around its outside, every edge moved in by the same width (world space). Same control as bevel: a dot where the mouse started and a line to it; away widens, back narrows. |
| Bevel | 1 vertex, edge, or face | A dot marks where the mouse started, with a line to the mouse; moving away widens the bevel, moving back narrows it. Works on open meshes too: a corner with two open edges gets cut off, cuts reaching an open edge slide along it, and beveling an open edge itself adds a strip on its face side while the outline stays put. |
| Delete | Vertices, edges, faces, lights, objects, or reference images | Removes them. |
| Connect | 2 vertices on one face | Adds an edge between them, splitting the face. |
| Fill | 1 border edge | Fills the open border loop it lies on with a new face. |
| Merge | 2 connected vertices | Radial menu (at the center) or `merge [center\|first\|last]`. Collapses the edge between them. |
| Dissolve | 1 edge or face | Radial menu or `dissolve`. Collapses an edge to its midpoint or a face to its center. |

Extrude and inset refuse selections where faces touch only at a corner, a region with no boundary (a closed surface), and a region with a hole. Refusals (from these, bevel, and dissolve) show as an error in the console and briefly in the status bar.

The tools work in world space, so they behave the same on a moved, rotated, or scaled object as on an untransformed one: the selection follows the mouse, X/Y/Z locks are world axes, and bevel and inset widths match the mouse distance.

Modal tools are confirmed with left click and cancelled with right click.

### Origins
- Every object's origin (the point its mesh is built around, and its own axes) shows as a dot on top of the scene in every mode: the active object's (the one being edited, in edit modes) bright, the others greyed. View ▸ Hide origins hides them all.
- Click an origin to select it, in any mode. A greyed one also makes its object the active one (or the one being edited) and clears the selection. Origins, lights, and mesh elements are never selected together, and only one origin at a time. Clicks prefer origins over lights over vertices.
- A selected origin is purple with short lines along its own +X, +Y, +Z (red, green, cyan).
- **Grab** (G, with X/Y/Z locks) moves the origin and **Rotate** (R) turns its axes while the mesh stays exactly where it is (its vertices shift the opposite way), and so do the object's children. Scale doesn't apply. Each is one undo step.
- With an origin selected, the radial menu shows the origin commands: **To geometry** (middle of the bounding box), **To bottom** (middle of the bottom, where an asset stands), **To world** (0, 0, 0), **Reset rotation** (axes back in line with the world). In edit mode, Edit ▸ **Origin here** puts it at the average of the selected vertices. All are undoable and also in the console as `origin geometry | bottom | world | rotation | selection`.
- Export uses each object's origin as the asset's pivot, so set it where the asset should stand. Whether origins show is saved with the project.

### Parenting
Objects inside other objects, so an asset can have rigid parts (a sword in a hand, a turret on a base) and, later, a skeleton can carry meshes.
- A child's position, rotation, and scale are relative to its parent: move, turn, or scale the parent and its children come along, and the panel's fields for a child show the relative values.
- **No skew:** a parent's scale multiplies a child's scale along the child's own axes (as Unreal Engine does), so a child turned inside an unevenly stretched parent is stretched, never sheared. Setting and clearing parents is exact.
- **Set a parent:** Ctrl+P in object mode makes the selected objects children of the active one (the last clicked), or drag a row onto another in the Objects tab. **Clear it:** Alt+P, or drag the row onto the list's empty space. Either way the child stays where it is in the world. An object can't become a child of itself or of one of its own children. Each is one undo step.
- The Objects tab is a tree: children indented under their parent, with an arrow that folds a family away. While dragging a row, a label follows the mouse and the row it would go under is outlined.
- In object mode a faint dashed line runs from each child's origin to its parent's.
- Clicking an object selects just it. Grabbing, scaling, or rotating a parent carries its children; with a parent and its child both selected, the child only moves with its parent.
- Deleting a parent keeps its children: they move up a level (to the grandparent, or the top), staying where they are.
- Moving or turning a parent's origin leaves its children where they are (see [Origins](#origins)).
- Saved in projects; exported as one file per top-level object with all its children (the Export window lists top-level objects, their children greyed under them, and a selected child checks its family); importing rebuilds the family.
- `object <id> parent <id | none>` in the console; `object list` shows each object's parent.

### Reference images
Pictures in the scene to model against (a front and side view of a character, a photo of a prop).
- **Add** with + in the panel's Images tab (pick one or more PNG files) or `reference add [<file.png>]`. A new image is 2 units tall, as wide as its proportions say, and stands at the camera's target facing the view, selected.
- The picture is copied into the project, so the project still opens if the file is moved or deleted.
- Each image is a flat plane locked to its picture's proportions: one **size** (its height) sets how big it is. It has a position and rotation like an object, and both sides show (the back mirrored).
- **Select** by clicking it, in any mode (Shift+click adds or removes), or in the Images tab's list. Origins and lights win clicks over images; between an image and a mesh, whichever is in front gets the click. A selected image is outlined in purple.
- **Grab** and **Rotate** with X/Y/Z axis locks, and **Scale**, which always keeps the proportions (axis locks don't apply). Delete removes it. All undoable.
- **Opacity** (0 to 1, on top of the picture's own transparency), **depth**: Behind (under everything, like a backdrop), Scene (hidden behind what's in front of it, the default), or Front (over everything), **Visible**, and **Locked**: a locked image ignores clicks in the viewport, so you can work on the model through it; it can still be picked in the list, to edit or unlock it.
- **Images tab:** + and −, a list (rows say "locked" or "hidden"), and the selected image's name, file and size in pixels, Visible, Locked, Opacity, Depth, Position, Rotation, and Size.
- Saved in the project and part of undo; never exported to `.vlmobj`.
- Pictures are read by a PNG reader written for this app (`shared/image/`, with its own decompression): every color type and bit depth, with transparency. Each one becomes a GPU texture with mipmaps the first time it's drawn.
- `reference` console command: list, add, remove, show/hide, lock/unlock, and set the name, opacity, depth, position, rotation, and size (see [systems/console.md](systems/console.md#reference-command)).

### Radial menu
Hold the thumb side button (Mouse4) to open a radial menu at the cursor, move toward a slice, and release to run it. Releasing in the middle, or right clicking, closes it without doing anything. While it's open, the camera, tools, and panel hold still.

- A menu's items split the circle evenly, with no empty slots: the first item is centered straight up and the rest go clockwise. Even counts mirror left/right and up/down; odd counts mirror only left/right. Items that can't run right now are dimmed, never removed, so positions stay put. Each label shows its key shortcut faintly.
- Moving past the ring toward a submenu (`>`) opens it, re-centered at the cursor. Two levels at most.
- Drawn with ring slices in the UI shader; the hovered slice is purple.

| Menu | Items, clockwise from the top |
|---|---|
| Main (8) | Grab, Scale, Edit ▸ (Light ▸ with lights selected), View ▸, Mode ▸, Redo, Undo, Rotate |
| With an origin selected (8) | Grab, To geometry, To bottom, View ▸, Mode ▸, To world, Reset rotation, Rotate |
| Edit ▸ (10) | Extrude, Inset, Fill, Dissolve, Delete, Merge, Connect, Bevel, Origin here, Shading ▸ |
| Edit ▸ Shading ▸ (6) | Smooth, Auto smooth, Mark hard, Clear mark, Mark smooth, Flat (the marks need edges selected in edge mode) |
| Light ▸ (5) | Spot, Directional, On/Off, Delete, Point |
| Mode ▸ (4) | Edge, Face, Object, Vertex |
| View ▸ (6) | Hide/Show panel, Headlight off/on, Debug on/off, Hide/Show origins, Clay view/Materials, UV grid/Hide UV grid (each label says what picking it will do) |
| While grab/scale/rotate/bevel/inset runs (4) | Y (up), Z (right), Free (down), X (left) |

Axis items toggle like the X/Y/Z keys; Free clears every lock. In the Light menu, a type is dimmed when every selected light already has it, and On/Off turns them all off if all are on, otherwise all on.

The menu replaces the awkward key chords (like M+V and M+F for modes). Everything else keeps its keyboard shortcut and isn't meant to move into the menu: Shift/Ctrl/Alt+click selection, Tab, the console, and confirm/cancel on the mouse buttons.

### Materials
How surfaces look, as in the game: the metallic-roughness set glTF, Unreal, and Unity use, so they export as they are.
- A material has a **base color**, **roughness** (sharp to no highlights), **metallic**, **emissive** color and **glow** strength, **opacity** with an **alpha mode** (Opaque; Cutout, drawn where opacity reaches a cutoff; Blend, see-through), and **both sides** (double-sided).
- Materials belong to the project and are shared: ten rocks can use "Granite", and editing it changes all ten. There's always a **Default** (the old gray, roughness 0.5, not metallic) that can be edited but not removed; objects without a material use it, and new objects start on it. Removing a material puts its objects back on Default; undo brings it back.
- **Assign** per object: the Objects tab's Material dropdown (with swatches), **Assign to selected** in the Materials tab (the selected objects in object mode, the object being edited otherwise), or `material <id> assign [<object ids>]`.
- **Per face:** in face mode, Assign gives the material to the selected faces (several materials on one object: a sword's blade and grip); **Use object's material** takes it off again, and **Select its faces** selects every face showing the material. Faces without their own use the object's, so changing the object's material still recolors them. The Objects tab notes how many faces have their own.
- **New faces** follow one rule: extrude, inset, and bevel give new faces the material of the faces they came from when those all share one, otherwise the object's; reshaped faces and extrude tops keep their own; fill takes what the faces around the hole share; connect gives both halves the split face's; dissolve and merge keep the surviving faces'. Removing a material puts its faces back on their object's.
- **Materials tab:** a list with each material's swatch and how many objects use it, + and −, and the selected material's large swatch and every value. Swatches are lit spheres rendered with the viewport's own shader (see-through ones over a checkerboard), updated as you edit. Every change is one undo step.
- **Shading:** each light adds diffuse plus a highlight shaped by roughness (GGX), metals tint their reflections with their color, and surfaces reflect a simple environment made from the background gradient (a sky above, a ground below, blurrier as roughness rises), so metals aren't black without an environment image. Emissive glows on top, then exposure and tone mapping.
- **Transparency:** see-through objects draw after everything solid, sorted farthest first together with reference images in the scene, without hiding what's behind them; a double-sided one shows its far side through its near side. To save work, Blend at full opacity draws as opaque, Blend at 0 and a Cutout below its cutoff aren't drawn, and the saved and exported values stay as set.
- **Material and clay view** (View ▸ Clay view / Materials, or `ui materials`): material view shows materials as the game will, with back faces culled unless a material is double-sided (clicks skip culled faces too); clay view shows everything in the Default gray with back faces tinted pink, to find mistakes while modeling. Saved with the project.
- **Smooth shading** (Edit ▸ Shading in the radial menu, the Objects tab's Shading row, or `shading`): each object is **Flat** (every face its own facet), **Smooth** (faces blend together), or **Auto** (blends faces that meet at less than an angle, 30° by default, and keeps sharper edges crisp). To decide exactly where creases go, select edges in edge mode and **Mark hard** (always a crease) or **Mark smooth** (always blended, even past the Auto angle); **Clear mark** goes back to the object's setting. In edit mode every edge that ends up hard is drawn in cyan, so the creases are visible while modeling. Marks follow split, extrude, inset, and bevel where the edge stays. Saved with the project and exported: the baked normals carry the shading, and re-importing restores the mode, angle, and marks.
- **UVs and the UV grid** (View ▸ UV grid, or `ui checker`): every face corner has a texture coordinate, and every preset comes laid out (the cube as a cross, cylinders and cones with their sides wrapped and caps beside them, spheres and the torus by their rings). Extrude, inset, bevel, splitting, connecting, and filling keep them. The UV grid paints every face with a colored checker read from its UVs, so stretching and seams show. Saved in the project and exported in `.vlmobj` (as a `uv0` attribute, and exactly in the editable polygons).
- Each object draws in one call per material it uses (its triangles are grouped by material on the GPU), and draws in a row with the same material send it once.
- Saved in the project, per face too; exported in `.vlmobj` (each file carries the materials its objects and faces use, one mesh part per material, and re-importing restores each face's); importing reuses a project material that's identical (name and every value) and otherwise adds the asset's, renamed if the name is taken ("Granite 2").
- `material` console command: list, add, remove, edit every value, and assign (see [systems/console.md](systems/console.md#material-command)).

### Undo / redo
- Ctrl+Z / Ctrl+Y, up to 100 steps.
- Snapshots all objects, materials, lights (including ambient), reference images, and the selection, so adding and removing them is undoable too. Images share their picture between snapshots, so undo doesn't copy it.
- A whole drag of a panel slider or field is one undo step.
- Opening a project or starting a new one clears the history.

### Projects
- Save and open Valuma Studio projects (`.vlm`): Ctrl+S (asks where the first time), Ctrl+Shift+S (Save As), Ctrl+O, Ctrl+N (new), with the Windows file dialogs; or the `save`, `open`, `new` commands with an optional path. Projects go in `Documents\Valuma Studio` by default: the dialogs start there, and console names like `save scene` land there. `fileinfo` lists a file's sections.
- A project holds the objects (whole half-edge meshes, with their parents and materials), the materials, lights and ambient light, reference images (with their pictures), the camera, the active object, the selection mode, and the view (headlight, exposure, material or clay view, back-face tint, debug overlay, panel position, size, and tab). The selection and undo history aren't saved.
- The window title shows the file name with a `*` when there are unsaved changes (anything undoable; undoing back to the saved state clears it). New, Open, and closing the window ask "Save changes?" first, with Save / Don't Save / Cancel in the app's own prompt.
- Binary format with a section per object, each checksummed; objects are written and read on several threads when there's enough geometry. Saves go to a temporary file that replaces the old one, and a damaged file is refused without touching the open scene. Sections a newer version adds are skipped. See [systems/project.md](systems/project.md).

### Assets (.vlmobj)
Finished assets go out to the game engine (Aevora) and back in as **`.vlmobj`**: one file is one complete asset, everything the engine needs for a monster, tree, or rock except the game code. A project can hold many assets (say, rocks that share materials) and export any of them.
- **Export** (Ctrl+E, not while a tool runs): a window lists every top-level object with a checkbox (an object starts checked when it or one of its children is selected; in an edit mode, the object being edited), its children greyed underneath, the file it will write (`<object name>.vlmobj`, characters Windows doesn't allow become `_`), and its status: **new**, **exists** (orange), or **duplicate** (another checked object has the name). Colliding rows get Replace (default) / Rename / Skip; a header row checks everything and sets every colliding row at once. Duplicates always rename (`Rock 2.vlmobj`). The footer says what will happen; **Export N** (or Enter) writes the files, each safely through a temporary file, and Cancel (or Escape) closes. A summary goes to the console. Re-exporting is Ctrl+E, Enter.
- Each file holds an object and all its children, and the materials they use (see [Materials](#materials)). The object's origin is the pivot: its position is dropped, its rotation and scale kept, so it looks exactly as it does around its origin. See [systems/vlmobj.md](systems/vlmobj.md).
- **Built for loading:** a header, a table of contents, and independent, checksummed, versioned sections on 64-byte boundaries with offsets only, so the engine can memory-map a file, hand the vertex and index data straight to the GPU, and read sections on several threads. Meshes are baked at export: triangulated, flat shaded like the viewport, vertices with the same position and normal shared, 16- or 32-bit indices, bounds. An editor-only section keeps the original polygons so re-importing keeps n-gons. Sections a newer version adds are skipped; materials, textures, skeletons, animations, attachment points, and hitboxes and collision shapes have sections reserved, added with those areas.
- The export folder (Browse… picks another) defaults to `Exports` next to the project and then remembers the last folder exported to, saved in the `.vlm` relative to the project when it's nearby. It doesn't count as an unsaved change.
- **Import** (Ctrl+I, Import… at the end of the Objects tab's dropdown then +, or `import [<path>]`): adds each picked `.vlmobj` as a new object named after the file, placed like a new preset, with its n-gons, rotation, and scale. Undoable.
- The format lives in a shared library (`shared/vlmobj/`) the Aevora engine will use too.

### Interface
- **Modal windows** (the Export window and prompts such as "Save changes?"): centered over a dimmed backdrop; while one is open, the panel, viewport, and shortcuts are off, Enter picks the default button (outlined) and Escape cancels.
- **Color picker** (light, ambient, headlight, and material colors): click a swatch to open a saturation/value square with a hue strip, a hex field (`#c08a4f`), and typed R/G/B values; the swatch shows the color it had when opened beside the new one. A drag or a typed value is one undo step.
- **Floating panel** (shown by default, `ui panel` toggles it): drag it by its header or tabs, resize it from any edge or corner (resize cursors, 240 × 160 minimum), and scroll it with the wheel or its scrollbar when the content doesn't fit. It always stays inside the viewport.
- **Objects tab** (first):
  - A preset dropdown (cube, plane, grid, circle, cylinder, cone, UV sphere, ico sphere, torus, and Import… for `.vlmobj` files), + to add that preset (or import), − to remove the object being edited.
  - A fixed-height object list that scrolls on its own; click a row to edit that object (synced with clicking it in the viewport). In object mode it shows and sets the selected objects.
  - The selected object: name field, vertex/edge/face counts, position, rotation, and scale fields, and its material (a dropdown with swatches).
- **Materials tab:** see [Materials](#materials).
- **Lights tab:**
  - A type button (click to cycle point → spot → directional), + to add that type, − to delete the selected light (dimmed when nothing is selected).
  - A fixed-height light list that scrolls on its own; click a row to select the light (synced with the viewport).
  - The selected light: name field, type switch, enabled, color, intensity, and the position, range, direction, and cone fields that apply to its type.
  - Ambient light, headlight, and exposure.
- **Images tab:** reference images (see [Reference images](#reference-images)).
- Clicks and scrolling over the panel don't reach the viewport.
- Long names in lists and labels that don't fit are shortened with "..." instead of running into their neighbors, so a narrow panel stays readable.
- **Text fields** (object and light names): click to edit with everything selected, type to replace it. Arrow keys, Home/End (Shift extends the selection), Backspace/Delete, Ctrl+A, and Ctrl+C/X/V with the system clipboard; click inside to place the caret or drag to select. Enter or a click anywhere else keeps the edit (one undo step), Escape restores the old text, and an empty name is ignored. While editing, keyboard shortcuts are off, so typing `g` or `/` just types.
- **X/Y/Z fields**: drag to change; click without dragging (under 3 px of movement) to type an exact value, with the same keys. Text that isn't a number leaves the value alone. A value too wide for its box shows fewer decimals rather than overlapping the axis letter (the panel is 360 px wide by default, enough for values like −12.50).
- Held keys repeat after the system's repeat delay, in text fields and in the console.
- **Status bar** along the bottom: frames per second, selection mode, active tool, and axis lock (`-` outside grab/scale/rotate, `Free`, or the locked axes in red/green/cyan). Items keep fixed positions as values change. An error that happens while the console is closed (an unknown command, a refused bevel/extrude/inset/dissolve) shows in red at the right end for 4 seconds, fading out at the end.
- **Console** (/): a panel docked above the status bar with commands, their output, and errors (red) above an input line with a blinking caret. `help` lists every command; an unknown command shows an error; tools report refusals there too (bevel, extrude, inset, dissolve). Long lines wrap. Output or errors longer than one line can be collapsed and expanded by clicking; closing the console collapses everything so far, so only new or reopened ones show expanded. The wheel scrolls the list; command history (Up/Down, with the recalled command highlighted in the list, which scrolls to keep it in view) and cursor movement; Backspace, Delete, and the arrow keys repeat while held. Commands: `help`, `save`, `open`, `new`, `import`, `origin`, `fileinfo`, `debug`, `validate`, `merge`, `dissolve`, `light`, `object`, `material`, `reference`, `exposure`, `stats`, `headlight`, `backface`, `vsync`, `ui`. See [systems/console.md](systems/console.md).
- Built on a from-scratch immediate-mode UI (tabs, buttons, dropdowns, list rows, checkboxes, sliders, X/Y/Z and single number fields, text fields, color swatches, type switches, scrolling list boxes) and a batched 2D draw list. See [systems/ui.md](systems/ui.md).
- Two embedded fonts: the 16×24 console font and a 10×16 UI font (the console font trimmed and scaled down).

### Mesh
- Half-edge polygon mesh with generational handles.
- N-gon faces, triangulated with ear clipping for rendering (cached per face).
- Topology validator (`validate` command) that reports the first broken invariant.
- Built-in presets: cube, plane, grid, circle, cylinder, cone, UV sphere, ico sphere, torus (see [systems/mesh.md](systems/mesh.md#presets)). The startup object is a cube; add more from the Objects tab or `object add <preset>`.

### Platform and build
- Native Win32 window, input, clipboard, and file and folder dialogs; OpenGL 3.3 via GLAD. No other third-party code.
- The window is titled Valuma Studio.
- Fonts and shaders are embedded into the executable with `#embed`.
- CMake build: `modeling` executable, `modeling_core` static library, the `vlmobj` and `image` libraries shared with the future engine, and a `tests` executable.
