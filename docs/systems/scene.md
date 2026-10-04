# Scene

What's being edited and how you look at it: objects, lights, transforms, the camera, the selection, picking, and undo history.

Files: `src/scene/` (meshes are covered separately in [mesh.md](mesh.md))

## Scene

[scene.hpp](../../src/scene/scene.hpp)

```cpp
struct Scene {
    Camera camera;
    ObjectCollection objects;
    LightCollection lights;
    Selection selection;
};
```

## Objects

[object_collection.hpp](../../src/scene/objects/object_collection.hpp)

```cpp
struct Object {
    std::string name;
    Transform transform;
    MeshData meshData;      // editable half-edge mesh
    OpenGLMesh gpuMesh;     // GPU copy, rebuilt when meshDirty
    bool meshDirty = true;
};
```

Set `meshDirty = true` after any change to `meshData`. `renderFrame` then calls `gpuMesh.update(meshData)` before drawing.

`ObjectCollection` is a `std::vector<Object>` wrapper. Objects are addressed by index, and selection and picking store that index.

| Method | Description |
|---|---|
| `create(name, PresetMesh)` | Appends an object with a preset mesh. **Can move every existing `Object`** (vector growth), so don't hold `Object&` across it. |
| `get(index)` | Object by index. |
| `count()` | Number of objects. |
| `all()` | The underlying vector. |

## Lights

[light.hpp](../../src/scene/lights/light.hpp), [light_collection.hpp](../../src/scene/lights/light_collection.hpp)

Besides the individual lights, the collection holds one `AmbientLight` (color and strength only). The ambient light and enabled directional lights shade mesh faces: each frame `renderFrame` copies them into a `LightingState` for the renderer (see [renderer.md](renderer.md#object-drawing)). Point and spot lights aren't used for shading yet, and no light is drawn in the viewport.

The default scene (`loadTestScene`) adds one directional light, `sun`, traveling along `(0.4, −1, −0.6)` (from above, front, left) at intensity 0.6.

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
| `count()` | Number of live lights. |
| `getAmbient()` | The scene's single ambient light. |
| `setAmbientColor(color)` / `setAmbientStrength(s)` | Set the ambient light; values are clamped to 0..1. |

Lights are part of the undo snapshot (see [History](#history)), so wrapping a light edit in `history.begin`/`commit` makes it undoable.

## Transform

[transform.hpp](../../src/scene/transform.hpp)

`position`, `rotation` (Euler angles in radians), `scale`. `getMatrix()` returns `T × Rz × Ry × Rx × S`. There's no UI to change transforms yet, but picking, bevel, and the debug overlay all apply them.

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

Separate lists of selected vertices, edges, and faces. Each entry stores an object index and a handle (`VertexSelection`, `EdgeSelection`, `FaceSelection`).

| Method | Description |
|---|---|
| `clear()` | Empties all three lists. |
| `addVertex/Edge/Face(object, handle)` | Adds if not already present. |
| `removeVertex/Edge/Face(object, handle)` | Removes if present. |
| `hasVertex/Edge/Face(object, handle)` | Membership test. |
| `hasVertices/Edges/Faces()` | Any selected. |
| `getVertices/Edges/Faces()` | Entries with object indices. |
| `getVertexHandles/EdgeHandles/FaceHandles()` | Handles only (object index dropped). |
| `getNumberOfVertices/Edges/Faces()` | Counts. |
| `setSelectionStartPositions(positions)` / `getSelectionStartPositions()` | Vertex positions saved when a modal tool starts, in the same order as `getVertices()`. Used to cancel or axis-snap. |

Conventions kept by the application code:
- Selecting an edge or face also selects its vertices. Tools that move geometry only look at the vertex list.
- An edge counts as selected if either of its half-edges is in the list.

## Picking

[ray.hpp](../../src/scene/selection/ray.hpp), [scene_queries.hpp](../../src/scene/selection/scene_queries.hpp)

| Function | Description |
|---|---|
| `makeRayFromScreenPosition(mouseX, mouseY, width, height, camera)` | World-space ray through a pixel, built by unprojecting the near and far planes. |
| `pickVertex(scene, ray, radius)` | Nearest vertex within `radius` (world units) of the ray. |
| `pickEdge(scene, ray, radius)` | Nearest edge within `radius` of the ray. |
| `pickFace(scene, ray)` | Nearest face whose triangulation the ray hits. |

All three test every object (with its transform) and return a hit struct: `hit`, `objectIndex`, the handle, and `distance` along the ray. The selection code uses a radius of 0.03.

## History

[history.hpp](../../src/scene/history.hpp)

Undo/redo by snapshot. A `State` is a copy of every object's `MeshData` and `Transform`, the whole `LightCollection`, and the `Selection`. Up to 100 undo steps are kept.

| Method | Description |
|---|---|
| `begin(scene)` | Captures a pending snapshot. Does nothing if one is already pending, so nested `begin` calls are safe. |
| `commit()` | Pushes the pending snapshot onto the undo stack and clears redo. |
| `cancel(scene)` | Restores the pending snapshot and discards it. |
| `undo(scene)` / `redo(scene)` | Swaps the current state with the top of the undo / redo stack. Returns false if empty. |

Usage pattern: call `begin` before changing anything, then exactly one of `commit` or `cancel`. Modal tools call `begin` when they start and `commit`/`cancel` when they end.

Restoring sets `meshDirty` on every object. History only restores objects that still exist, and doesn't add or remove objects. Lights are restored wholesale, so adding and removing lights is undoable.
