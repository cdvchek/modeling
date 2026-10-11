# Mesh

Editable polygon meshes stored as a half-edge structure. All mesh code is in `valuma_core`, has no OpenGL or Win32 dependencies, and is covered by the tests.

Files: `valuma/src/scene/mesh/`

| File | Contents |
|---|---|
| [mesh_handles.hpp](../src/scene/mesh/mesh_handles.hpp) | `VertexHandle`, `EdgeHandle`, `FaceHandle`, `INVALID_*` constants |
| [mesh_types.hpp](../src/scene/mesh/mesh_types.hpp) | `Vertex`, `Edge`, `Face`, `Triangle`, `PackagedMesh`, `PresetMesh` |
| [mesh_data.hpp](../src/scene/mesh/mesh_data.hpp) | `MeshData` class, GPU export structs, `SlideSession` |
| `mesh_data_access.cpp` | Element lookup, handle lists, `setMesh` |
| `mesh_data_queries.cpp` | Topology traversal, loops and rings |
| `mesh_data_geometry.cpp` | Positions, normals, triangulation, dirty flags |
| `mesh_data_primitives.cpp` | Private low-level building blocks |
| `ops/mesh_data_ops.cpp` | Extrude, split, remove, fill, connect |
| `ops/mesh_data_merge.cpp` | Edge collapse and vertex merge |
| `ops/mesh_data_dissolve.cpp` | Dissolve edge / face |
| `ops/mesh_data_bevel.cpp` | Vertex / edge / face bevel; `replaceFaces`; `runInSpace` |
| `mesh_data_shading.cpp` | Smooth shading: mode, angle, edge marks, hard edges, corner normals, faces to patch |
| `ops/mesh_data_region.cpp` | Region extrude and inset |
| `mesh_data_validate.cpp` | Topology checker |
| `mesh_data_gpu.cpp` | Flattening to vertex/index arrays for the renderer |
| [mesh_factory.hpp](../src/scene/mesh/mesh_factory.hpp), `presets/*.cpp` | Built-in meshes and the `fromPolygons` builder |

## Handles and DynamicArray

Mesh elements are never referenced by pointer or plain index. A `Handle<Tag>` is `{ u32 index, u32 generation }`; `VertexHandle`, `EdgeHandle`, and `FaceHandle` are distinct types, so they can't be mixed up.

Element storage is `DynamicArray<T, HandleT>` from [core/containers/dynamic_array.hpp](../../shared/core/containers/dynamic_array.hpp), a generic slot array that can hold any type. `HandleT` defaults to `Handle<T>`, so `DynamicArray<Light>` hands out `Handle<Light>` with no tag struct needed; the mesh uses explicit tags (`VertexTag` etc.) instead.

How it works:
- `insert(value)` reuses a free slot if there is one, otherwise appends. Returns a handle with the slot's current generation.
- `remove(handle)` marks the slot free and **increments its generation**, so every old handle to it becomes invalid.
- `isValid(handle)` is true only if the slot is in use and the generation matches.
- `get(handle)` asserts validity; `tryGet(handle)` returns `nullptr` instead.
- `getActiveHandles()` / `getActiveValues()` list live elements in slot order.
- `size()` is the slot count (including free slots); `activeSize()` is the live count.
- `reserve(count)` makes room for that many slots up front.

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
    Vec2 uv;            // UV of its face's corner at its tip
    EdgeMark mark;      // None, Hard, or Smooth; the same on both halves
    bool seam;          // a UV seam: unwrapping cuts here; the same on both halves
};

struct Vertex {
    Vec3 position;
    EdgeHandle edge;    // any one outgoing half-edge
};

struct Face {
    EdgeHandle edge;    // any one half-edge on its loop
    MaterialHandle material;    // its own material; not valid (none, or removed) means the object's
    mutable std::vector<Triangle> triangles;    // cached triangulation
    mutable bool triangulationDirty;
};
```

**Keeping the GPU copy up to date:** `stamp()` gives a `MeshStamp`: the vertex, edge, and face arrays' stamps (every `DynamicArray` insert or remove takes a new number from `nextStructureStamp`, unique for the whole run, and copies keep theirs) a face-material stamp (`setFaceMaterial`), and a UV stamp (`setFaceUVs` / `setCornerUVs`, which rebuild the GPU copy), and a shading stamp (the mode, angle, or a mark changed). An equal stamp means the same layout. `positionVertex` and `translateVertex` (and bevel and inset widths, which go through them) record the vertex in `movedVertices()`, switching to `allMoved()` once the list passes about twice the vertex count; `clearMoved()` empties it after the GPU copy is updated, and `markAllMoved()` says every vertex may differ (a restored copy, a cancelled bevel or inset). `getVertexFaces(vertex)` lists the faces around a vertex, and `appendFaceCorners(face, out)` writes a face's corners exactly as `getFaceData` does (it's what `getFaceData` uses), so a patched face matches a rebuilt one bit for bit. `getFacesToPatch(moved)` lists the faces a move changes: those around the moved vertices, and with smoothing also every face around their corners, since a turned face changes its neighbours' corner normals.

**Face materials:** `getFaceMaterial(face)` / `setFaceMaterial(face, material)`; `sharedFaceMaterial(faces)` is the material they all share, or `INVALID_MATERIAL` when they differ. Operations give new faces a material by one rule: the faces they're built from share one, the new faces get it, otherwise they use the object's. Extrude, inset, and bevel go through `replaceFaces`, whose first `oldFaces.size()` new faces replace the old ones in order (reshaped faces and extrude tops keep their own materials) and whose other new faces (sides, strips, corner fills) follow the rule; extruding a single face (`insertFaceRing`) gives its sides the face's material; `fillFaceLoop` uses the faces across the hole (or a material it's given); `connectVertices` gives both halves the split face's material; dissolve and merge keep the surviving faces'.

**UVs:** every face corner has a UV, stored on the half-edge that points to that corner (`Edge::uv`), so a vertex on a seam can hold a different UV in each face. They follow the glTF convention: (0, 0) is the image's top-left corner and v runs down. `getFaceUVs(face)` lists them in the same order as `getFaceVertices(face)` (each half-edge's tip); `setFaceUVs(face, uvs)` sets them. `getCornerUVs()` / `setCornerUVs(uvs)` read and write every half-edge's UV in handle order (border half-edges included), for saving. `getUVIsland(face)` gives the face's island: the faces reachable across edges whose two sides have the same UVs at both ends (within 1e-5), itself included; a seam (UVs apart) separates islands.

**UV seams:** `isSeam(edge)` / `setSeam(edge, seam)` (both halves), `getSeamEdges()` (one half each), `getFaceEdgeSeams(face)` (by corner, for export), `getEdgeSeams()` / `setEdgeSeams()` / `hasSeams()` (for saving), and `markSeamsFromIslands()`, which marks a seam on every edge between two faces whose UVs differ there (the current layout's cuts) and returns how many. `splitEdge` gives both pieces the seam; `replaceFaces` keeps the seam of an edge rebuilt between the same two vertices (or of the outside half it pairs with), as it does marks; `fromPolygons` takes `faceSeams` like `faceMarks`.

### Unwrapping

[uv_unwrap.hpp](../src/scene/mesh/uv_unwrap.hpp) (`namespace UVUnwrap`, part of `valuma_core`). Every function works on the faces it's given; the rest of the mesh's UVs stay as they are. `toWorld` (the object's world matrix) makes sizes and directions the ones you see.

| Function | Description |
|---|---|
| `seamIslands(mesh, faces)` | The faces in pieces: joined across edges that aren't seams, both faces in the list. |
| `uvIslands(mesh, faces)` | The faces in the pieces their UVs form (`getUVIsland`, kept to the list). |
| `unwrap(mesh, faces, toWorld)` | Flattens each seam island by least squares conformal maps: the unknowns are corner groups (a face's corners joined with their neighbours' across non-seam edges, so a vertex on a seam that cuts in without cutting off can sit in two places), two rows per triangle measure how far the map is from a similarity, two points are pinned (the farthest apart along the island's longest side), and conjugate gradients on the normal equations (CGLS, no matrix formed) solve it from a flat projection as the starting guess. Each island is then laid the presets' way round (the winding a box projection of its largest triangle has; mirrored otherwise), turned to whichever of its own edge directions gives the smallest bounds (a square sits square), and scaled so its UV area equals its area in the world. Islands land near the origin; pack them after. Returns the islands. |
| `projectPlanar(mesh, faces, toWorld, right, up)` | u along `right`, v down along `up`. |
| `projectBox(mesh, faces, toWorld)` | Each face from whichever of the six axis directions its normal is closest to, seen from outside the way the presets are laid out (tops with the back at the top, bottoms with the front). |
| `projectCylinder(mesh, faces)` | Around the object's own Y axis through the faces' middle: u the angle, v down the height, in proportion (a full turn is the average circumference); faces crossing the wrap move round by a whole turn. |
| `projectSphere(mesh, faces)` | Like a globe around the faces' middle: u the angle, v from the top pole down (half a turn, in proportion), with the same wrap fix. |
| `pack(mesh, islands, area, margin, rotate)` | Arranges the islands inside `area` (a UV rectangle, 0 to 1 by default) in rows, tallest first, all scaled by one amount (so sizes keep their proportions), the largest that fits (found by halving), with `margin` (a fraction of the area's width) around each. `rotate` turns tall islands a quarter turn first (a rotation, never a mirror). |
| `bounds(mesh, faces, area)` | The UV bounds of the faces' corners. | Every operation keeps them: `insertFaceRing` gives the top its old corners' UVs and each side quad the UVs of the rim it rises from; `splitEdge` puts the midpoint UV on each side; `fillFaceLoop` takes the UV each corner has in the face across the hole (or `uvByVertex`, a vertex-slot → UV map it's given); `connectVertices` gives both halves the split face's UVs. `replaceFaces` (extrude, inset, bevel) gives each new face's corner the UV it had in its reference face (the old face it replaces, or for sides and fills the old face sharing the most corners), found by vertex, then by exact position, then in any old face, then the nearest old corner.

**Smooth shading:** each mesh has a `ShadingMode` (`getShading` / `setShading`): **Flat** (every face its own facet, the default), **Smooth** (faces blend into each other), or **Auto** (faces meeting at more than `getSmoothAngle()`, 30° by default, keep a hard edge). An edge's `EdgeMark` (`setEdgeMark`, which sets both halves) overrides Smooth and Auto: Hard always creases, Smooth always blends; Flat ignores marks. `isEdgeHard(edge)` gives the result (borders are always hard), and `getHardEdges()` lists one half of each hard edge between two faces (empty in Flat), which edit mode draws in cyan. `getCornerNormal(incoming)` is the normal of the corner at the half-edge's tip: the faces around that vertex reachable without crossing a hard edge, each face's normal weighted by its corner angle. The walk starts from the fan's first face (after a hard edge, or the lowest half-edge when the fan closes), so every corner in a fan gets the same normal bit for bit, which keeps exported vertices shared. One hard edge at a vertex doesn't split it; it takes two to cut its fan apart. `getFaceEdgeMarks(face)` lists the marks in corner order; `getEdgeMarks` / `setEdgeMarks` and `hasEdgeMarks` are for saving. `splitEdge` gives both pieces the edge's mark; `replaceFaces` keeps the mark of an edge rebuilt between the same two vertices (or of the outside half it pairs with), so extrude, inset, and bevel keep the marks of edges that don't move away; new edges start unmarked.

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
| `setMesh(PresetMesh)` | Replaces the mesh with a preset at its default settings (see [Presets](#presets)). |
| `setMesh(PackagedMesh)` | Replaces the mesh with one built elsewhere (`MeshFactory::fromPolygons`, an imported asset). |
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

### Regions (extrude and inset)

Both work on **regions**: selected faces joined by shared edges. `findRegions` groups the faces, then checks each region and returns a `RegionError` (text from `regionErrorText`):

| Error | When |
|---|---|
| `NoFaces` | Nothing valid selected |
| `CornerTouch` | Two regions share a vertex, or one region pinches (two of its boundary edges leave the same vertex) |
| `NoBoundary` | The region has no boundary edge (a closed surface) |
| `Holes` | The region's boundary is more than one loop |
| `Failed` | `replaceFaces` couldn't rebuild (shouldn't happen after the checks; the caller restores the mesh) |

`ringRegions` then, for every region: copies each boundary vertex, rebuilds the region's faces on the copies (interior vertices stay), and adds a quad `[a, b, copy b, copy a]` on each boundary edge `a → b`, all through `replaceFaces`. Mesh-border edges are fine: the quad pairs with the open border.

| Function | Description |
|---|---|
| `extrudeRegions(faces, topFaces)` | Builds the rings with the copies at the old positions (zero-height walls). `topFaces` are the region faces on the copies; the app grabs them. |
| `insetRegions(faces, session, innerFaces, space)` | Same rings, then fills a `SlideSession`: each copy slides into the region in the plane of its two boundary faces (the sum of both edges' inward directions, scaled so each edge moves in by exactly the width). `maxWidth` stops any copy halfway along a region edge leaving its corner. Runs in `space` like bevel, so widths are world units when the app passes the object's matrix. |

`insertFaceRing(face)` (single face, below) is the older one-face version, still used by tests.

### Bevel

Bevel is interactive, so it's split into a setup call and a width call that runs every frame. Inset uses the same `SlideSession`, `setSlideWidth`, and `cancelSlide`.

| Function | Description |
|---|---|
| `bevelVertex(vertex, session, space)` / `bevelEdge(edge, session, space)` / `bevelFace(face, session, space)` | Snapshot the mesh into `session`, then rebuild topology around the element with zero width. Fail on interior corners with fewer than 3 edges (border corners need 2) and on vertices with more than one border gap (two open edges pinched together). `space` (default identity) is the space the bevel is measured in: positions are moved into it, beveled, and brought back (`runInSpace`), with existing vertices restored from an exact copy so they don't drift. The session's starts and directions end up in mesh space, but widths and `maxWidth` are in `space` units. The app passes the object's matrix, so bevels are even in world space on scaled objects. |
| `setSlideWidth(session, width)` | Moves the new vertices to `start + direction × width`, clamped to `[0, session.maxWidth]`. `maxWidth` is set so vertices can't slide past neighboring geometry. |
| `cancelSlide(session)` | Restores the snapshot. |

```cpp
struct SlideSession {
    DynamicArray<...> savedVertices, savedEdges, savedFaces;  // snapshot for cancel
    std::vector<VertexHandle> vertices;  // vertices that move with width
    std::vector<Vec3> starts;            // their positions at width 0
    std::vector<Vec3> directions;        // unit slide directions
    f32 maxWidth;
};
```

Internally, `bevel()` classifies each corner's spokes as beveled, sliding, or plain, computes slide and inset directions, then builds the new faces with `replaceFaces()`.

**Border corners.** A vertex on a mesh border has an open fan: one outgoing spoke is the border half-edge with no face. `bevel()` rotates the spokes so that one comes first, which makes slot 0 the **gap** (no face) and the fan run from one border edge to the other:
- Spokes on either side of the gap aren't neighbors, so beveling one doesn't make the other slide, and no inset vertex is ever created in the gap.
- A border spoke next to a beveled spoke slides along the border, so cuts reach the outline.
- If a border edge itself is beveled, the corner is kept and sits in the gap: it is the gap side of that edge's strip and part of the corner's fill face. The outline doesn't move; the strip is one-sided.
- Whatever has no face on its other side (the gap side of a fill face, a chamfer with no fill) becomes new border: `replaceFaces(..., allowBorders = true)` gives such edges border twins, removes old border half-edges nothing uses anymore, and `relinkBorders()` relinks every border half-edge to the one leaving its tip. Without `allowBorders`, an unpaired edge is still an error (extrude, inset, and interior bevels keep that safety check).

### GPU export

Used by `OpenGLMesh` to build buffers. See [renderer.md](renderer.md#gpu-meshes).

| Function | Returns |
|---|---|
| `getVertexData()` | Flat `x,y,z` positions for live vertices, plus `indexMap` from vertex slot index to GPU vertex index. |
| `getEdgeData(vertexData)` | Line index pairs (one per edge, not per half-edge), plus a map from half-edge slot index to its offset in the index buffer. Both halves map to the same line. |
| `getFaceData(groupOf)` | Faces are sorted into **groups** by material first (`groupOf` maps a face's own material to its group, e.g. a removed material to `INVALID_MATERIAL`, the object's; without it, the handle as stored), so each group's triangles are one run of indices, listed in `groups` (`FaceGroup`: material, first index, count; the object's group first). The renderer draws one run per material. Then, for each face, its own vertex buffer: every triangle's three corners are written separately as `x, y, z, nx, ny, nz, u, v` (`FaceData::FLOATS_PER_VERTEX` = 8). In flat shading a planar face (`isFacePlanar`: every corner within 1e-5 of its size off the plane) uses its `getFaceNormal` on every triangle and a non-planar one each triangle's own (cross product of its edges), so it shows its fold along the triangulation; with smoothing every corner gets `getCornerNormal`. The `.vlmobj` bake reads these same corners. `indices` run over that buffer in order, wound counter-clockwise around the normal. `indexMap` holds `(offset, count)` pairs per face slot. |

### Files

[mesh_data_serialize.cpp](../src/scene/mesh/mesh_data_serialize.cpp), used by project files (layout in [project.md](project.md#file-format)).

| Function | Description |
|---|---|
| `writeTo(BinaryWriter&)` | Counts, then flat arrays: positions, each vertex's half-edge, each half-edge's tip/pair/next/prev/face, each face's half-edge. Deleted slots are packed out, so stored links are indices `0..n-1` (`INVALID_INDEX` for none). |
| `readFrom(BinaryReader&)` | Reads that back into fresh arrays, so element `i` gets handle `{ i, 0 }`. Every link is range-checked first; on a short read or a link out of range it returns false and leaves the mesh unchanged. It doesn't run `validate()` (the project loader does). |

Face order, winding, and each loop's starting half-edge are kept, so a mesh comes back the same apart from handle numbers. UVs, shading, and edge marks aren't part of this; the project file stores them next to the mesh.

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

Each preset is a `MeshFactory` function in `presets/` that returns a `PackagedMesh` (the three arrays); `setMesh(PresetMesh::...)` copies it in. All are centered on the origin and sized to match the 1-unit cube.

| `PresetMesh` | Function (defaults) | Shape |
|---|---|---|
| `Cube` | `cube()` | 8 vertices, 6 quads |
| `Plane` | `plane(size 1, divisions 1)` | One quad on the XZ plane facing +Y. Open: all edges are borders. |
| `Grid` | `grid(size 2, divisions 10)` | `plane` with 10 × 10 quads |
| `Circle` | `circle(radius 0.5, sides 32)` | A single 32-sided n-gon facing +Y. Open. |
| `Cylinder` | `cylinder(radius 0.5, height 1, sides 16)` | 16 side quads plus two 16-sided n-gon caps |
| `Cone` | `cone(radius 0.5, height 1, sides 16)` | 16 triangles meeting at an apex plus a 16-sided base |
| `UVSphere` | `uvSphere(radius 0.5, segments 16, rings 8)` | Quads, with triangle fans at the poles |
| `IcoSphere` | `icoSphere(radius 0.5, subdivisions 1)` | 80 triangles (icosahedron split once) |
| `Torus` | `torus(major 0.4, minor 0.15, segments 24, sides 12)` | 288 quads; every loop wraps around |

Every preset comes with UVs: the cube unfolds into a cross on a 4 × 4 grid (each side its own quarter-size cell), the plane, grid, and circle map straight down onto the square, the cylinder and cone wrap their sides around the top half and put each cap as a disc in the bottom half, the UV sphere and torus use their rings and segments, and the ico sphere is mapped like a globe (u around, v from pole to pole, with faces crossing the seam shifted past 1 so they don't stretch across the whole texture).

`setMesh` always uses the defaults; the parameters are there for preset options later (see [Ideas](features.md#good-next-picks)).

### fromPolygons

`MeshFactory::fromPolygons(positions, faces, faceUVs, faceMarks)` builds a half-edge mesh from a vertex list and faces given as vertex indices, **counter-clockwise seen from outside** (so `getFaceNormal` points out). Every preset uses it. `faceUVs` (optional) gives each face's corner UVs in the same order; a face whose list is missing or the wrong size gets zeros. `faceMarks` (optional) likewise gives the mark of the edge ending at each corner; border halves copy their pair's. Each face's first half-edge points to its second corner, so `getFaceVertices` starts there.

1. Inserts vertices in order, so a vertex's handle index equals its position index.
2. Creates each face's half-edge loop.
3. Pairs half-edges by their end vertices. A half-edge with no opposite gets a border half-edge (no face) as its pair.
4. Links border half-edges into hole loops.
5. Sets each vertex's outgoing edge, preferring a border one.

It asserts if two faces use the same directed edge (inconsistent winding or a non-manifold edge) or a border doesn't close into a loop.

To add a preset: write the function in a new `presets/*.cpp` using `fromPolygons`, declare it in `mesh_factory.hpp`, add a `PresetMesh` value and a `setMesh` case, add the file to `VALUMA_CORE_SRC` in `valuma/CMakeLists.txt`, and add a test to `preset_tests.cpp`.
