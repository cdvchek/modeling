# Scene

What's being edited and how you look at it: objects, lights, reference images, transforms, the camera, the selection, picking, and undo history.

Files: `src/scene/` (meshes are covered separately in [mesh.md](mesh.md))

## Scene

[scene.hpp](../../src/scene/scene.hpp)

```cpp
struct Scene {
    Camera camera;
    ObjectCollection objects;
    LightCollection lights;
    ReferenceCollection references;
    Selection selection;

    Object* activeObject();   // the object being edited, or null when there are none
};
```

## Objects

[object_collection.hpp](../../src/scene/objects/object_collection.hpp)

```cpp
struct Object {
    std::string name;
    Transform transform;    // relative to the parent (the world, for a top-level object)
    MeshData meshData;      // editable half-edge mesh
    ObjectHandle parent;    // INVALID_OBJECT when it has none
    bool meshDirty = true;  // set after any change to meshData
};
```

`Object` is scene data only. Its GPU copy lives in the application's `ObjectMeshCache` ([object_meshes.hpp](../../src/application/object_meshes.hpp)), keyed by handle, so objects can be copied freely (undo snapshots copy the whole collection). Set `meshDirty = true` after changing `meshData`; `renderFrame` rebuilds the GPU copy and clears the flag.

`ObjectCollection` stores objects in a `DynamicArray<Object>` and hands out `ObjectHandle` (`Handle<Object>`), the same pattern as lights. `INVALID_OBJECT` is the null handle. A removed object's handle stays invalid even if its slot is reused.

| Method | Description |
|---|---|
| `add(name, PresetMesh)` | Adds an object with a preset mesh and returns its handle. |
| `add(Object)` | Adds a whole object as it is (opening a project); sets `meshDirty`. |
| `remove(handle)` | Removes the object. |
| `get(handle)` / `tryGet(handle)` | Reference (asserts) / pointer or `nullptr`. |
| `isValid(handle)` | Whether the handle points to a live object. |
| `handles()` | Every live object, in slot order. |
| `handleAt(slot)` | The live object in that slot, or an invalid handle. The `object` command uses slot indices as ids. |
| `count()` | Number of objects. |
| `uniqueName(base)` | `base`, or `base N` with the lowest N that isn't taken. |
| `markAllDirty()` | Sets `meshDirty` on every object (after an undo restore). |
| `parentOf(h)`, `childrenOf(h)`, `isAncestor(a, h)`, `topLevelOf(h)` | The family: the parent (or `INVALID_OBJECT`), the direct children in slot order, whether `a` is above `h`, and the object at the top of `h`'s family. |
| `hierarchy()` | Every object as a `HierarchyEntry` (handle, depth): parents before their children, siblings in slot order. The Objects tab's tree. |
| `worldTransform(h)` / `worldMatrix(h)` | Where the object is in the world: its transform combined with every parent's (`combineTransforms`). A top-level object's is its own, returned as is. Drawing, picking, and every tool use these. |
| `parentWorldTransform(h)` | The parent's world transform, or the identity. |
| `setWorldTransform(h, world)` / `setWorldPosition(h, position)` | Put the object at a world transform (its relative one is worked out), or move only its origin, leaving its relative rotation and scale exactly as they are (grab and scale use this, so a long drag doesn't wear on the angles). |
| `setParent(child, parent)` | A new parent (`INVALID_OBJECT`: none), keeping the child where it is in the world. False, changing nothing, for an invalid handle or a parent that is the child or one of its children. |

`remove(h)` first moves the object's children up a level (to its parent, or the top), keeping them where they are.

### Active object

Only one object is edited at a time: `selection.getActiveObject()` (`scene.activeObject()` for the object itself). Selected vertices, edges, and faces always belong to it, and tools act on it. In edit modes only the active object draws its edges, vertices, and selection; the others draw just their lit faces. In object mode selected objects draw every edge in the selection color (a purple outline), and the others draw just their faces. A new object added in object mode becomes the selection.

Clicking in the viewport (any selection mode): the click is tested against the active object's elements for the current mode and its faces. If another object's face is closer than anything hit on the active object, that object becomes active instead, in the same mode with nothing selected. Loop and ring selection only look at the active object. The `object <id> edit` command also switches it.

## Lights

[light.hpp](../../src/scene/lights/light.hpp), [light_collection.hpp](../../src/scene/lights/light_collection.hpp)

Besides the individual lights, the collection holds one `AmbientLight` (color and strength only). All enabled lights shade mesh faces: each frame `renderFrame` copies them into a `LightingState` for the renderer (see [renderer.md](renderer.md#object-drawing)). Every light is drawn as a marker in the viewport (see [application.md](application.md)); `light list` prints them (see [console.md](console.md#light-command)).

The default scene (`loadTestScene`) adds one directional light, `Sun`, traveling along `(0.4, −1, −0.6)` (from above, front, left) at intensity 0.6.

```cpp
enum class LightType { Point, Directional, Spot };

struct Light {
    std::string name;
    LightType type = LightType::Point;
    Vec3 position;              // point and spot
    Vec3 direction;             // directional and spot
    Vec3 color;                 // linear RGB, default white
    f32 intensity = 1.0f;
    f32 range = 10.0f;          // point and spot
    f32 innerConeRadians;       // spot, half-angle
    f32 outerConeRadians;       // spot, half-angle
    bool enabled = true;
};

struct AmbientLight {
    Vec3 color;                 // default white
    f32 strength = 0.6f;
};
```

`LightCollection` stores lights in a `DynamicArray<Light>` and hands out `LightHandle` (`Handle<Light>`). A removed light's handle stays invalid even if its slot is reused. `INVALID_LIGHT` is the null handle.

| Method | Description |
|---|---|
| `add(light)` | Stores a copy and returns its handle. |
| `remove(handle)` | Removes the light. Does nothing for an invalid handle. |
| `replace(handle, light)` | Overwrites the light in that slot, keeping the handle. Returns false if the handle is invalid. |
| `get(handle)` | Reference to the light; edit fields through it. Asserts the handle is valid. |
| `tryGet(handle)` | Pointer, or `nullptr` if the handle is invalid. |
| `isValid(handle)` | Whether the handle points to a live light. |
| `handles()` | Handles of every live light, in slot order. |
| `handleAt(slot)` | Handle of the live light in that slot, or an invalid handle. The `light` command uses slot indices as light ids. |
| `count()` | Number of live lights. |
| `nextName()` | "Light N", with N one past the highest "Light N" in use (other names are ignored), so names don't repeat after deleting. |
| `getAmbient()` | The scene's single ambient light. |
| `setAmbientColor(color)` / `setAmbientStrength(s)` | Set the ambient light; values are clamped to 0..1. |

Lights are part of the undo snapshot (see [History](#history)), so wrapping a light edit in `history.begin`/`commit` makes it undoable.

## Reference images

[reference_image.hpp](../../src/scene/references/reference_image.hpp), [reference_collection.hpp](../../src/scene/references/reference_collection.hpp)

Pictures placed in the scene to model against (see [features.md](../features.md#reference-images)). They aren't objects: they have no mesh, aren't exported, and are selected on their own.

```cpp
struct ReferencePicture {          // the file as it was added; never changes
    std::string fileName;
    std::vector<u8> png;
    u32 width, height;             // pixels
};

struct ReferenceImage {
    std::string name;
    std::shared_ptr<const ReferencePicture> picture;   // shared by copies, so undo doesn't copy the file
    Vec3 position, rotation;       // rotation in Euler angles, applied like an object's
    f32 size = 2;                  // height in world units; the width follows the picture
    f32 opacity = 1;
    ReferenceDepth depth = InScene;   // Behind, InScene, InFront
    bool locked, visible = true;

    f32 aspect() const;            // width over height of the picture (1 without one)
    Transform transform() const;   // scale = (size × aspect, size, 1)
    Mat4 matrix() const;
};
```

The plane is the unit square from −0.5 to 0.5 in X and Y, facing its own +Z, top at +Y; `matrix()` stretches it to the picture's proportions, so they can't be skewed. `MIN_REFERENCE_SIZE` (0.001) keeps the size away from zero. `referenceDepthName(depth)` gives `"behind"`, `"scene"`, or `"front"`.

`ReferenceCollection` is a `DynamicArray<ReferenceImage>` with the same methods as the light collection (`add`, `remove`, `isValid`, `get`, `tryGet`, `handles`, `handleAt(slot)`, `count`) plus `uniqueName(base)` (`base`, or `base N` with the lowest free N).

## Transform

[transform.hpp](../../src/scene/transform.hpp)

`position`, `rotation` (Euler angles in radians), `scale`. `getMatrix()` returns `T × Rz × Ry × Rx × S`. Edited from the panel's Objects tab, the `object` command, and object-mode grab/scale/rotate.

`ObjectSpace(transform)` holds the model matrix and its inverse: `pointToWorld`, `pointToLocal`, and `directionToLocal` (ignores position). Edit-mode grab, scale, and rotate use it to work in world space on the active object's mesh, so mouse movement and axis locks follow the screen and the world axes on any object transform.

`combineTransforms(parent, local)` gives a child's world transform without skew: `position = parent.position + R(parent) × (parent.scale ⊙ local.position)`, `rotation = R(parent) × R(local)` (back to Euler angles), `scale = parent.scale ⊙ local.scale`, so a parent's scale lands on the child's own axes. `relativeTransform(parent, world)` is its exact reverse. See [Parenting](../features.md#parenting).

`eulerFromAxes(x, y, z)` gives the Euler angles that turn the X, Y, and Z axes onto three given unit axes (right-handed, at right angles); a new reference image uses it to face the view (`x` = camera right, `y` = camera up, `z` = back toward the camera).

`rotateEuler(euler, axis, angle)` returns the Euler angles after a further world-space turn of `angle` around `axis` (builds both rotations as 3×3 matrices, multiplies, and reads the angles back; looking straight along Y, where X and Z turn about the same axis, it puts the whole turn in Z). Object-mode rotate uses it.

## Camera

[camera.hpp](../../src/scene/camera.hpp)

An orbit camera around `target`.

| Member | Description |
|---|---|
| `position`, `target`, `up` | View setup. Up is +Y. |
| `distance`, `yaw`, `pitch` | Orbit parameters. Zoom changes `distance`. |
| `fovRadians`, `nearPlane`, `farPlane` | 60°, 0.1, 1000. |
| `updatePositionFromOrbit()` | Recomputes `position` from target, distance, yaw, pitch. Call after changing any of them. |
| `getViewMatrix()` / `getProjectionMatrix(aspect)` | Look-at and perspective matrices. |
| `getForward()` / `getRight()` | Unit view direction and right vector. Used by grab, pan, and rotate. |

Orbit, pan, and zoom input is handled in `checkSelectionContext` (see [application.md](application.md#checkselectioncontext)). Panning moves `position` and `target` together.

## Selection

[selection.hpp](../../src/scene/selection/selection.hpp)

Separate lists of selected vertices, edges, faces, lights, objects, and reference images, plus the **active object**. Element entries store the object's handle and the element handle (`VertexSelection`, `EdgeSelection`, `FaceSelection`); they always belong to the active object.

| Method | Description |
|---|---|
| `clear()` | Empties the element, light, object, and reference image lists and the selected origin; the active object stays. |
| `addVertex/Edge/Face(object, handle)` | Adds if not already present. |
| `removeVertex/Edge/Face(object, handle)` | Removes if present. |
| `hasVertex/Edge/Face(object, handle)` | Membership test. |
| `hasVertices/Edges/Faces()` | Any selected. |
| `getVertices/Edges/Faces()` | Entries with object indices. |
| `getVertexHandles/EdgeHandles/FaceHandles()` | Element handles only (object dropped). |
| `getActiveObject()` / `setActiveObject(handle)` | The object being edited. Changing it clears the selected elements (and another object's selected origin); `clear()` leaves it alone. |
| `selectOrigin(object)`, `clearOrigin()`, `hasOrigin()`, `getOrigin()` | One object's origin, selected on its own. `selectOrigin` makes that object active and clears elements, lights, and objects; adding a vertex, edge, face, light, or object clears the origin. So origins never mix with anything else, enforced here rather than by the callers. |
| `getNumberOfVertices/Edges/Faces()` | Counts. |
| `addLight/removeLight/hasLight(handle)`, `hasLights()`, `getLights()` | Selected lights. Independent of the selection mode; `clear()` empties them too. |
| `selectObject/deselectObject/hasObject(handle)`, `hasObjects()`, `getObjects()`, `clearObjects()` | Whole objects selected in object mode. Changing the active object doesn't touch them. |
| `addReference/removeReference/hasReference(handle)`, `hasReferences()`, `getReferences()`, `clearReferences()` | Selected reference images. `addReference` clears every other kind (elements, lights, objects, the origin), and adding any other kind clears the images, so they never mix with anything, enforced here. |
| `setReferenceStartTransforms(transforms)` / `getReferenceStartTransforms()` | Reference image placements saved when grab, scale, or rotate starts, in the same order as `getReferences()` (`scale.y` holds the size). |
| `setObjectStartTransforms(transforms)` / `getObjectStartTransforms()` | Object transforms saved when grab, scale, or rotate starts, in the same order as `getObjects()`. Used to rebuild each frame and to cancel. |
| `clearMeshElements()` / `clearLights()` | Clear one kind only. Lights and mesh elements are never selected together: the click handlers in `check_selection.cpp` call `clearMeshElements` when a light is picked and `clearLights` when a vertex, edge, face, loop, or ring is picked (with or without Shift). An origin excludes both, enforced by `Selection` itself. |
| `setLightStartPositions(positions)` / `getLightStartPositions()` | Light positions saved when grab starts, in the same order as `getLights()`. Used to cancel or axis-snap. |
| `setLightStartDirections(directions)` / `getLightStartDirections()` | Light directions saved when rotate starts, same order. Used to cancel or reset on axis lock. |
| `setSelectionStartPositions(positions)` / `getSelectionStartPositions()` | Vertex positions saved when a modal tool starts, in the same order as `getVertices()`. Used to cancel or axis-snap. |

Conventions kept by the application code:
- Selecting an edge or face also selects its vertices. Tools that move geometry only look at the vertex list.
- An edge counts as selected if either of its half-edges is in the list.

## Origins

[origin.hpp](../../src/scene/objects/origin.hpp)

An object's origin is its transform: the point its mesh is built around and the axes it turns on. These change it while the mesh stays where it is in the world, and so do its children. Origins are given as world transforms.

| Function | Description |
|---|---|
| `captureOrigin(objects, h)` | An `OriginStart`: the origin's world transform, the vertex positions, and each direct child's world transform. |
| `setOrigin(objects, h, start, to)` | Puts the origin at the world transform `to`, moves every vertex by `inverse(new world) × start world` so each keeps its world position, and puts each child back at its start world transform. Working from the start each time keeps a long drag from drifting. Marks the mesh dirty. |
| `setOrigin(objects, h, to)` | The same, from the object as it is now. |
| `vertexPositions(mesh)` | Every vertex position in `getVertexHandles` order. |
| `originAtCenter(objects, h)` | Target: the middle of the mesh's bounding box, in its own axes. |
| `originAtBottom(objects, h)` | Target: the middle of the bounding box's bottom (lowest along the object's own Y). |
| `originAtVertices(objects, h, vertices)` | Target: their average. |
| `originAtWorld(objects, h)` | Target: the world's 0, 0, 0, axes unchanged. |
| `originAlignedToWorld(objects, h)` | Target: rotation reset to 0 in the world, position unchanged. |

Targets keep the object's rotation and scale unless they say otherwise.

## Picking

[ray.hpp](../../src/scene/selection/ray.hpp), [scene_queries.hpp](../../src/scene/selection/scene_queries.hpp)

| Function | Description |
|---|---|
| `makeRayFromScreenPosition(mouseX, mouseY, width, height, camera)` | World-space ray through a pixel, built by unprojecting the near and far planes. |
| `pickVertex(scene, ray, radius, only)` | Nearest vertex within `radius` (world units) of the ray. |
| `pickEdge(scene, ray, radius, only)` | Nearest edge within `radius` of the ray. |
| `pickFace(scene, ray, only, exclude)` | Nearest face whose triangulation the ray hits. With `exclude`, finds another object under the mouse. |
| `pickLight(scene, viewProjection, mouseX, mouseY, width, height, radius)` | Nearest light whose marker is within `radius` pixels of the mouse, measured on screen. Returns a `LightHit` with the handle and pixel distance. Checked before the mesh picks (after origins); the radius is `LIGHT_MARKER_PICK_RADIUS` (11 px) from `light_markers.hpp`. |
| `pickOrigin(scene, viewProjection, mouseX, mouseY, width, height, radius)` | The object whose origin projects nearest the mouse within `radius` pixels (`OriginHit`: object and pixel distance). Checked first of all, when origins show; the radius is `ORIGIN_MARKER_PICK_RADIUS` (9 px) from `origin_markers.hpp`. |

| `pickReference(scene, ray)` | The reference image the ray meets (either side), skipping hidden and locked ones (`ReferenceHit`: handle, its depth setting, and the distance along the ray). Images set to Front win over the rest and images set to Behind lose to the rest, whatever their distance; within a group the nearest wins. The click code then compares it with the mesh under the mouse (see [application.md](application.md#checkselectioncontext)). |

The mesh picks test every object (with its transform), or only `only` when it's valid, skipping `exclude`. They return a hit struct: `hit`, `object` (handle), the element handle, and `distance` along the ray. The selection code uses a radius of 0.03.

## History

[history.hpp](../../src/scene/history.hpp)

Undo/redo by snapshot. A `State` is a copy of the whole `ObjectCollection`, the whole `LightCollection`, the whole `ReferenceCollection` (cheap: pictures are shared, not copied), and the `Selection` (including the active object). Up to 100 undo steps are kept.

| Method | Description |
|---|---|
| `begin(scene)` | Captures a pending snapshot. Does nothing if one is already pending, so nested `begin` calls are safe. |
| `commit()` | Pushes the pending snapshot onto the undo stack and clears redo. |
| `cancel(scene)` | Restores the pending snapshot and discards it. |
| `undo(scene)` / `redo(scene)` | Swaps the current state with the top of the undo / redo stack. Returns false if empty. |
| `stateId()` | Names the current state: each commit gives a new id, and undo or redo back to a state gives its id again. The app compares it with the id at the last save to show unsaved changes (see [project.md](project.md)). `cancel` keeps the id. |
| `clear()` | Drops every step (after opening a project or starting a new one); the state gets a new id. |

Usage pattern: call `begin` before changing anything, then exactly one of `commit` or `cancel`. Modal tools call `begin` when they start and `commit`/`cancel` when they end.

Restoring replaces objects, lights, and reference images wholesale and sets `meshDirty` on every object, so adding, removing, and editing objects and lights are all undoable.
