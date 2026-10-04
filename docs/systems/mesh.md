# Mesh

Editable polygon meshes stored as a half-edge structure. All mesh code is in `modeling_core`, has no OpenGL or Win32 dependencies, and is covered by the tests.

Files: `src/scene/mesh/`

| File | Contents |
|---|---|
| [mesh_handles.hpp](../../src/scene/mesh/mesh_handles.hpp) | `VertexHandle`, `EdgeHandle`, `FaceHandle`, `INVALID_*` constants |
| [mesh_types.hpp](../../src/scene/mesh/mesh_types.hpp) | `Vertex`, `Edge`, `Face`, `Triangle`, `PackagedMesh`, `PresetMesh` |
| [mesh_data.hpp](../../src/scene/mesh/mesh_data.hpp) | `MeshData` class, GPU export structs, `BevelSession` |
| `mesh_data_access.cpp` | Element lookup, handle lists, `setMesh` |
| `mesh_data_queries.cpp` | Topology traversal, loops and rings |
| `mesh_data_geometry.cpp` | Positions, normals, triangulation, dirty flags |
| `mesh_data_primitives.cpp` | Private low-level building blocks |
| `mesh_data_ops.cpp` | Extrude, split, remove, fill, connect |
| `mesh_data_merge.cpp` | Edge collapse and vertex merge |
| `mesh_data_dissolve.cpp` | Dissolve edge / face |
| `mesh_data_bevel.cpp` | Vertex / edge / face bevel |
| `mesh_data_validate.cpp` | Topology checker |
| `mesh_data_gpu.cpp` | Flattening to vertex/index arrays for the renderer |
| [mesh_factory.hpp](../../src/scene/mesh/mesh_factory.hpp), `presets/cube.cpp` | Built-in meshes |

## Handles and DynamicArray

Mesh elements are never referenced by pointer or plain index. A `Handle<Tag>` is `{ u32 index, u32 generation }`; `VertexHandle`, `EdgeHandle`, and `FaceHandle` are distinct types, so they can't be mixed up.

Element storage is `DynamicArray<T, HandleT>` from [core/containers/dynamic_array.hpp](../../src/core/containers/dynamic_array.hpp), a generic slot array that can hold any type. `HandleT` defaults to `Handle<T>`, so `DynamicArray<Light>` hands out `Handle<Light>` with no tag struct needed; the mesh uses explicit tags (`VertexTag` etc.) instead.

How it works:
- `insert(value)` reuses a free slot if there is one, otherwise appends. Returns a handle with the slot's current generation.
- `remove(handle)` marks the slot free and **increments its generation**, so every old handle to it becomes invalid.
- `isValid(handle)` is true only if the slot is in use and the generation matches.
- `get(handle)` asserts validity; `tryGet(handle)` returns `nullptr` instead.
- `getActiveHandles()` / `getActiveValues()` list live elements in slot order.
- `size()` is the slot count (including free slots); `activeSize()` is the live count.

`INVALID_VERTEX`, `INVALID_EDGE`, `INVALID_FACE` have index `INVALID_INDEX`; `handle.isNull()` checks for that.

Because handles survive copying, a `MeshData` snapshot (used by undo and bevel cancel) keeps every handle in the selection valid after restoring.

## Half-edge structure

Every edge is two **half-edges** pointing in opposite directions.

```cpp
struct Edge {           // a half-edge
    EdgeHandle pair;    // the opposite half-edge
    EdgeHandle next;    // next half-edge around the same face (or hole)
    EdgeHandle prev;    // previous half-edge around the same face
    VertexHandle tip;   // vertex this half-edge points to
    FaceHandle face;    // face on its left, or INVALID_FACE on a border
};

struct Vertex {
    Vec3 position;
    EdgeHandle edge;    // any one outgoing half-edge
};

struct Face {
    EdgeHandle edge;    // any one half-edge on its loop
    mutable std::vector<Triangle> triangles;    // cached triangulation
    mutable bool triangulationDirty;
};
```

- A half-edge's origin is `pair.tip` (`getEdgeOrigin`).
- Following `next` from any half-edge walks a closed **loop**: a face's boundary, counter-clockwise, or a border hole if `face` is invalid.
- Walking `pair.next` from a vertex's outgoing edge visits every outgoing edge around it (its **fan**).
- A **border** half-edge has no face. Its pair does.

### Invariants

`MeshData::validate()` checks all of these and prints the first one that fails:

- Every half-edge has a valid pair that points back, and the two have different tips.
- `next`/`prev` are valid and consistent, `next` starts where this edge ends, and all edges in a loop share the same face.
- Every vertex with edges has a valid outgoing `edge`, and its fan returns to the start and reaches every outgoing edge.
- Every face points at an edge on its own loop, has at least 3 sides, and doesn't visit a vertex twice.

Every operator must leave the mesh passing `validate()`, including when it fails.

## MeshData API

### Access

| Function | Description |
|---|---|
| `setMesh(PresetMesh)` | Replaces the mesh with a preset (currently only `Cube`). |
| `getVertex` / `getEdge` / `getFace(handle)` | Pointer to the element, or `nullptr` if the handle is invalid. |
| `getVertices()` / `getFaces()` | Copies of all live elements. |
| `getVertexHandles()` / `getEdgeHandles()` / `getFaceHandles()` | Handles of all live elements. Edge handles include both halves. |
| `getEdgeOrigin(edge)` / `getEdgeTip(edge)` | Endpoints of a half-edge. |
| `isValidHandle(handle)` | Overloaded for all three handle types. |

### Topology queries

| Function | Description |
|---|---|
| `getFaceVertices(face)` | Corners in loop order. |
| `getOutgoingEdges(vertex)` / `getIncomingEdges(vertex)` | Half-edges leaving / arriving at a vertex. |
| `getLoopEdges(edge)` | All half-edges on the loop containing `edge` (a face or a hole). |
| `isBorder(edge)` | The half-edge has no face. |
| `isBorderVertex(vertex)` | Any outgoing half-edge or its pair is a border. |
| `getEdgeLoop(edge)` | Edge loop: continues straight through vertices with exactly 4 edges, in both directions. Stops at other vertices or borders. A border edge returns its whole hole. |
| `getEdgeRing(edge)` | Edge ring: crosses each quad to the opposite edge, in both directions. Stops at non-quads. |
| `getFaceLoop(edge)` | The quads crossed by the same walk as `getEdgeRing`. |
| `validate()` | See [Invariants](#invariants). |

Private helpers: `getFaceEdges`, `getVertexNeighbors`, `findEdge(origin, tip)`, `walkRing`.

### Geometry

| Function | Description |
|---|---|
| `getVertexPosition(vertex)` | Position, or (0,0,0) if invalid. |
| `getFaceNormal(face)` | Unit normal by Newell's method (works for non-planar n-gons). Zero for degenerate faces. |
| `getFaceTriangles(face)` | Cached triangulation; re-triangulates if `triangulationDirty`. |
| `positionVertex(vertex, pos)` / `translateVertex(vertex, delta)` | Move a vertex. **Doesn't mark faces dirty**: call `setFacesDirtyByVertex` too. |
| `setFacesDirtyByVertex` / `ByEdge` / `ByFace` | Mark every face touching the element for re-triangulation. |

**Triangulation** (`triangulateFace`, private) is ear clipping: compute the normal, project to 2D by dropping the dominant axis, find the winding, then repeatedly clip valid ears.

### Operators

All return `false` / an invalid handle when they refuse, and leave the mesh unchanged and valid.

| Function | Description |
|---|---|
| `insertFaceRing(face)` | Extrude/inset core: duplicates the face's vertices, builds a quad on each side, and moves the original face handle to the new top loop. New vertices start at the old positions; the caller moves them. Returns the top face. |
| `splitEdge(edge)` | Inserts a vertex at the midpoint. Both adjacent faces gain a corner. Returns the new vertex. |
| `removeVertex(vertex)` | Removes every edge on the vertex, then the vertex. |
| `removeEdge(edge)` | Removes both adjacent faces, then the edge pair, merging their loops into one hole. |
| `removeFace(face)` | Removes the face only; its loop becomes a border hole. |
| `fillFaceLoop(edge)` | Creates a face on the border hole `edge` (or its pair) is on. Fails if that loop already has a face. |
| `connectVertices(a, b)` | Adds an edge between two vertices on the same loop. If that loop is a face, splits it into two faces. Fails if they're already connected or don't share a loop. |
| `mergeVertices(a, b, type)` | Collapses the edge `a–b`. `type` 0 = midpoint, 1 = at `a`, 2 = at `b`. `a` survives. Requires an edge between them. |
| `dissolveEdge(edge)` | Collapses the edge to its midpoint. Returns the surviving vertex. |
| `dissolveFace(face)` | Collapses every corner into one vertex at the face center. Restores the mesh if any step is refused. |

**Edge collapse** (`canCollapseEdge`, `collapseEdge`, `collapseSide`, private) is shared by merge and dissolve. Triangles on either side of the edge collapse into a single edge. `canCollapseEdge` refuses collapses that would break the mesh: two triangles with the same apex, shared neighbors that aren't triangle apexes, a loop touching both ends other than through the edge, or pinching two borders together through the interior.

### Bevel

Bevel is interactive, so it's split into a setup call and a width call that runs every frame.

| Function | Description |
|---|---|
| `bevelVertex(vertex, session)` / `bevelEdge(edge, session)` / `bevelFace(face, session)` | Snapshot the mesh into `session`, then rebuild topology around the element with zero width. Fail on corners with fewer than 3 edges or any border edge. |
| `setBevelWidth(session, width)` | Moves the new vertices to `start + direction × width`, clamped to `[0, session.maxWidth]`. `maxWidth` is set so vertices can't slide past neighboring geometry. |
| `cancelBevel(session)` | Restores the snapshot. |

```cpp
struct BevelSession {
    DynamicArray<...> savedVertices, savedEdges, savedFaces;  // snapshot for cancel
    std::vector<VertexHandle> vertices;  // vertices that move with width
    std::vector<Vec3> starts;            // their positions at width 0
    std::vector<Vec3> directions;        // unit slide directions
    f32 maxWidth;
};
```

Internally, `bevel()` classifies each corner's spokes as beveled, sliding, or plain, computes slide and inset directions, then builds the new faces with `replaceFaces()`.

### GPU export

Used by `OpenGLMesh` to build buffers. See [renderer.md](renderer.md#gpu-meshes).

| Function | Returns |
|---|---|
| `getVertexData()` | Flat `x,y,z` positions for live vertices, plus `indexMap` from vertex slot index to GPU vertex index. |
| `getEdgeData(vertexData)` | Line index pairs (one per edge, not per half-edge), plus a map from half-edge slot index to its offset in the index buffer. Both halves map to the same line. |
| `getFaceData()` | Its own vertex buffer: every face's corners are written separately as `x, y, z, nx, ny, nz` (`FaceData::FLOATS_PER_VERTEX` = 6), with the face's `getFaceNormal` as the normal. That gives flat shading: a vertex shared by three faces appears three times with three normals. `indices` are the face triangulations into that buffer, wound counter-clockwise around the normal. `indexMap` holds `(offset, count)` pairs per face slot. |

### Private primitives

Low-level steps used to build operators. They don't keep the mesh valid on their own.

| Function | Description |
|---|---|
| `addVertex(pos)` | New vertex with no edges. |
| `addEdgePair(origin, tip)` | New paired half-edges, unlinked, no face. |
| `deleteEdgePair(edge)` | Removes both halves. |
| `link(a, b)` | Sets `a.next = b` and `b.prev = a`. |
| `spliceOut(edge)` | Links `prev` to `next`, skipping the edge. |
| `assignFace(start, face)` | Sets `face` on every half-edge in the loop, and `face.edge = start`. |
| `repairVertexEdge(vertex)` / `repairFaceEdge(face)` | Point the element at a surviving edge after deletions. |
| `retargetIncoming(from, to)` | Makes every half-edge pointing at `from` point at `to`. |

## Presets

`MeshFactory::cube()` returns a `PackagedMesh` (the three arrays), which `setMesh(PresetMesh::Cube)` copies in. To add a preset: add a value to `PresetMesh`, a `MeshFactory` function in `presets/`, a case in `setMesh`, and the new `.cpp` to `CORE_SRC`.
