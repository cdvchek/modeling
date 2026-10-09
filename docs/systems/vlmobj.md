# Asset format (.vlmobj), version 1

**Status: version 1, implemented** in the shared library ([shared/vlmobj/](../../shared/vlmobj/)) with Valuma's baking and rebuilding ([asset_file.cpp](../../src/asset/asset_file.cpp)); exported from the Export window (Ctrl+E) and imported with Ctrl+I (see [In the app](#in-the-app)). Once Aevora reads these files, changing a layout means a new version.

A `.vlmobj` file is one finished asset (a monster, a tree, a rock) as Valuma Studio exports it for the Aevora engine: everything the engine needs except the game code. It's built for loading. The engine can memory-map the file, check a few numbers, and hand the vertex and index data straight to the GPU, with no parsing and no rebuilding. Everything slow (triangulating, computing bounds) happens once, at export.

Valuma's own project files (`.vlm`, see [project.md](project.md)) are a different format: they hold editable half-edge meshes and editor state. A `.vlmobj` is what comes out of a project.

## Design rules

- **Little-endian** throughout; no byte swapping on the machines we target.
- **Fixed-size records with no hidden padding.** Every record is laid out so a C++ struct with the same fields (checked with `static_assert` on its size) maps straight onto the bytes. Records are aligned to their largest field.
- **Offsets, never pointers.** Everything is found through offsets, so the file can be used where it lies in memory.
- **Sections start on 64-byte boundaries** (zero padding between them). Data inside a section that the GPU takes (vertex and index data) starts on 16-byte boundaries.
- **Independent sections**, each with its own version and checksum, so readers can load them on separate threads and skip what they don't need.
- **Unknown sections are skipped**, so files with sections added later still load in older readers. A known section with a newer version than the reader understands is refused.
- **Fixed conventions:** right-handed, +Y up, 1 unit = 1 meter, triangles wound counter-clockwise when seen from the front.
- **"None" is `0xFFFFFFFF`** wherever an index can be empty (`NONE` below).
- **Strings are UTF-8** in one string table, referenced by offset and length.

## Layout

```
Header        64 bytes, at offset 0
Directory     one 48-byte entry per section, at directoryOffset (64 in files Valuma writes)
Sections      each on a 64-byte boundary, in any order
```

### Header (64 bytes)

| Offset | Type | Field | Value |
|---|---|---|---|
| 0 | `char[4]` | magic | `VLMO` |
| 4 | `u32` | version | `1` |
| 8 | `u32` | headerSize | `64`; later versions may grow it, readers skip the rest |
| 12 | `u32` | flags | `0` (none defined) |
| 16 | `u32` | sectionCount | |
| 20 | `u32` | directoryCrc | CRC-32 of the whole directory |
| 24 | `u64` | directoryOffset | `64` |
| 32 | `u64` | fileSize | the file's total size, to catch truncated files cheaply |
| 40 | `f32` | metersPerUnit | `1.0` |
| 44 | `u8` | upAxis | `1` (+Y) |
| 45 | `u8` | handedness | `0` (right-handed) |
| 46 | `u16` | reserved | `0` |
| 48 | `u32` | headerCrc | CRC-32 of bytes 0–47 |
| 52 | `u8[12]` | reserved | `0` |

### Directory entry (48 bytes)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `u32` | type | Four characters, first one in the low byte, so it reads as text in a hex dump (`NODE` is the bytes `N O D E`) |
| 4 | `u32` | version | The section's own version; all are `1` here |
| 8 | `u64` | offset | From the start of the file; a multiple of 64 |
| 16 | `u64` | storedSize | Bytes in the file |
| 24 | `u64` | size | Bytes after decompression; equals `storedSize` when not compressed |
| 32 | `u32` | crc | CRC-32 of the stored bytes |
| 36 | `u8` | compression | `0` = none. The only value in version 1; reserved for a fast LZ-style scheme later (textures) |
| 37 | `u8` | flags | bit 0: **editor only**, so the engine can skip the section without looking at its type |
| 38 | `u16` | reserved | `0` |
| 40 | `u32` | count | How many records the section holds (nodes, meshes, …), so readers can allocate before reading; `0` where it doesn't apply |
| 44 | `u32` | reserved | `0` |

Version 1 has at most one section of each type; readers use the first one they find.

### String references

```
StringRef (8 bytes):  u32 offset   into the STRS section
                      u32 length   in bytes, without the terminating zero
```

An empty or missing string is `{ 0, 0 }`. Each string in `STRS` is followed by a zero byte, so `offset` also works as a C string.

## Sections in version 1

| Type | Required | Holds |
|---|---|---|
| `STRS` | yes | The string table |
| `NODE` | yes | The asset's parts and how they're arranged |
| `MESH` | if any node has a mesh | One record per mesh: counts, vertex layout, where its data is, bounds |
| `PART` | with `MESH` | Ranges of a mesh's indices, one per material |
| `MATL` | if any part has a material | Materials: base color, roughness, metallic, emissive, opacity, alpha mode, sides, base color texture |
| `TEXR` | if any material has a texture | Textures: each one's record, then its PNG file |
| `VTXS` | with `MESH` | Vertex data for every mesh |
| `IDXS` | with `MESH` | Index data for every mesh |
| `EDIT` | no (editor only) | Editable polygons and exact rotations, so re-importing into Valuma keeps n-gons |

### STRS: string table

Raw bytes: strings one after another, each followed by a zero byte. The first byte of the section is a zero, so offset 0 is always the empty string.

### NODE: nodes (64 bytes each)

The asset is a tree of nodes. **Node 0 is the root.** Nodes are stored parents first (every node's `parent` is a lower index), so the engine can compute world transforms in one pass.

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `StringRef` | name | The object's name in Valuma |
| 8 | `u32` | parent | Node index, or `NONE` for the root |
| 12 | `u32` | mesh | Index into `MESH`, or `NONE` |
| 16 | `f32[3]` | translation | Relative to the parent |
| 28 | `f32[4]` | rotation | Unit quaternion `x, y, z, w`, relative to the parent |
| 44 | `f32[3]` | scale | |
| 56 | `u32` | flags | `0` (none defined) |
| 60 | `u32` | reserved | `0` |

A node's transform is relative to its parent. **Combining a node with its parent** (the rule every reader must use; `vlmobj::combine` in the shared library):

```
world.translation = parent.translation + parent.rotation × (parent.scale ⊙ local.translation)
world.rotation    = parent.rotation × local.rotation
world.scale       = parent.scale ⊙ local.scale          (⊙: component by component)
```

So a parent's scale multiplies a child's along the **child's own axes**, and nothing is ever skewed (Unreal Engine combines transforms the same way; glTF multiplies matrices instead, so a converter would bake world transforms). A node's mesh is drawn with its world `translation × rotation × scale` (scale first). The root's world transform is its own.

**What Valuma writes in version 1:** the exported object as node 0, the root, then everything under it, parents first, one mesh per node. The root's translation is `0, 0, 0` because its origin is the asset's pivot; its rotation and scale are its world ones, so the asset looks exactly as it did around its origin. Every other node keeps its transform relative to its parent, as in Valuma.

### MESH: meshes (160 bytes each)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `StringRef` | name | |
| 8 | `u32` | vertexCount | |
| 12 | `u32` | indexCount | A multiple of 3 |
| 16 | `u64` | vertexOffset | Into `VTXS`; a multiple of 16 |
| 24 | `u64` | indexOffset | Into `IDXS`; a multiple of 16 |
| 32 | `u32` | vertexStride | Bytes per vertex |
| 36 | `u8` | indexSize | `2` (u16) or `4` (u32). Valuma uses `2` when `vertexCount` ≤ 65,536 |
| 37 | `u8` | attributeCount | Up to 8 |
| 38 | `u8` | primitive | `0` = triangle list (the only value in version 1) |
| 39 | `u8` | reserved | `0` |
| 40 | `u32` | firstPart | Into `PART` |
| 44 | `u32` | partCount | |
| 48 | `f32[3]` | boundsMin | Bounding box in the mesh's own space |
| 60 | `f32[3]` | boundsMax | |
| 72 | `f32[3]` | sphereCenter | Bounding sphere, same space |
| 84 | `f32` | sphereRadius | |
| 88 | `VertexAttribute[8]` | attributes | The first `attributeCount` are used; the rest are zero |
| 152 | `u8[8]` | reserved | `0` |

**VertexAttribute** (8 bytes):

| Offset | Type | Field |
|---|---|---|
| 0 | `u8` | semantic |
| 1 | `u8` | format |
| 2 | `u16` | reserved (`0`) |
| 4 | `u32` | offset within a vertex, in bytes |

| semantic | | format | |
|---|---|---|---|
| 1 | position | 1 | `f32 ×2` |
| 2 | normal | 2 | `f32 ×3` |
| 3 | tangent | 3 | `f32 ×4` |
| 4 | uv0 | 4 | `f16 ×2` |
| 5 | uv1 | 5 | `snorm16 ×2` (packed normals) |
| 6 | color | 6 | `unorm8 ×4` |
| 7 | joints | 7 | `u8 ×4` |
| 8 | weights | 8 | `u16 ×4` |
| | | 9 | `unorm16 ×4` |

Valuma writes position and normal (both `f32 ×3`) and `uv0` as `f32 ×2`. The rest are listed so the numbers are fixed now: tangents come with normal maps, joints and weights with Animation, and the packed formats are a later export option for smaller files. A reader should refuse a mesh whose attribute uses a semantic or format it doesn't know.

**What Valuma writes:** stride 32, `position` at 0, `normal` at 12, `uv0` at 24. That's the layout Valuma's renderer already uses. UVs follow the glTF convention: (0, 0) is the image's top-left corner, v runs down. A vertex is split wherever its corners have different UVs (a seam), the same way it's split where normals differ.

**Vertex sharing:** export merges vertices whose attributes are exactly the same (bit for bit), and the indices point to the shared ones. Shading is flat, as in the viewport, so a vertex is a position plus the normal of the face it's on:
- A planar face gives all its triangles the face's normal, so they share corners: a quad is 4 vertices and 6 indices, a cube 24 vertices, and a flat grid shares across neighbouring faces (a 10×10 grid is 121 vertices).
- A face that isn't planar keeps a normal per triangle, as the viewport draws it, so its fold looks the same in the engine. Those triangles only share where their normals happen to match.
- Faces at an angle to each other never share, since their normals differ; that's what keeps the edges sharp.

Smooth shading (planned on its own) will only change the data, not the format: corners around a vertex get the same averaged normal and merge across faces, while edges marked hard keep different normals and stay split, by the same rule.

### PART: mesh parts (16 bytes each)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `u32` | firstIndex | Into the mesh's indices |
| 4 | `u32` | indexCount | A multiple of 3 |
| 8 | `u32` | material | Index into the `MATL` section, or `NONE` for the engine's default material |
| 12 | `u32` | reserved | `0` |

One part is one draw call. Valuma writes one part per material a mesh uses: its triangles are sorted so each material's are one run (faces without their own material use the object's).

### MATL: materials (64 bytes each, version 2)

The metallic-roughness set glTF and the engines use, so the engine can build its own material from it directly.

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `StringRef` | name | |
| 8 | `f32[3]` | baseColor | sRGB, 0 to 1 |
| 20 | `f32` | roughness | 0 mirror-sharp highlights, 1 none |
| 24 | `f32` | metallic | 0 plastic, stone, wood; 1 metal |
| 28 | `f32[3]` | emissiveColor | sRGB, 0 to 1 |
| 40 | `f32` | emissiveStrength | Multiplies `emissiveColor`; 0 doesn't glow |
| 44 | `f32` | opacity | 0 to 1 |
| 48 | `f32` | alphaCutoff | Cutout only: drawn where opacity reaches it |
| 52 | `u8` | alphaMode | 0 opaque (opacity ignored), 1 cutout, 2 blend (see-through, sorted, no depth writes) |
| 53 | `u8` | flags | Bit 0: double-sided (back faces drawn, lit with the normal flipped); otherwise back faces are culled |
| 54 | `u16` | reserved | `0` |
| 56 | `u32` | baseColorTexture | Version 2: index into `TEXR`, or `NONE`. Its color multiplies `baseColor` and its alpha multiplies `opacity`, read through `uv0`. In version 1 this was reserved (`0`) and means none, so check the section version (`File::baseColorTexture` does) |
| 60 | `u32` | reserved | `0` |

**Colors are stored as authored, in sRGB** (what color pickers show), not linear as glTF's factors are, so a material read back into Valuma is bit-for-bit the one exported. Convert them to linear once when loading (the standard sRGB curve). Lighting should match Valuma's viewport: Lambert diffuse plus GGX highlights (Smith visibility, Schlick Fresnel with 0.04 reflectance for non-metals), emissive added on top, then tone mapping (Khronos PBR Neutral) and sRGB output; see [renderer.md](renderer.md#object-drawing).

A file only holds the materials its own parts use. Several assets can each carry a copy of the same material; the engine can recognize them by name and values.

### TEXR: textures (64-byte records, then their data)

The section starts with `count` records; each texture's bytes follow, every block starting on a 16-byte boundary. Offsets are from the start of the section.

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | `StringRef` | name | |
| 8 | `u32` | width | Pixels |
| 12 | `u32` | height | Pixels |
| 16 | `u8` | format | `1` PNG: the data is a whole PNG file, colors sRGB (an engine decodes it with any PNG reader, such as `shared/image`) |
| 17 | `u8` | flags | `0` |
| 18 | `u16` | reserved | `0` |
| 20 | `u32` | reserved | `0` |
| 24 | `u64` | dataOffset | From the start of the section, past the records |
| 32 | `u64` | dataSize | Bytes |
| 40 | `u32[6]` | reserved | `0` |

Any size is allowed, powers of two or not. PNG keeps files small; a later format (pixels with mipmaps made ahead, ready to upload) can be added as another `format` value without changing the record. Textures are smooth-filtered (linear with mipmaps) and repeat outside 0 to 1. A file only holds the textures its materials use; several materials can share one.

### VTXS and IDXS: vertex and index data

Raw bytes. Each mesh's vertices are `vertexCount × vertexStride` bytes at its `vertexOffset`. Its indices are `indexCount × indexSize` bytes at its `indexOffset`, and they index that mesh's vertices only. Keeping all vertex data in one section and all index data in another means the engine can upload each in one go and draw meshes by offset.

### EDIT: editable polygons (editor only, version 4)

Lets Valuma rebuild the exact mesh it exported, with n-gons, face order, winding, and materials. The engine skips it (directory flag bit 0).

```
u32  nodeCount
f32  eulerRotation[3 × nodeCount]   each node's rotation as Valuma stores it (radians, X then Y then Z),
                                    since turning the quaternion back into angles might pick different ones
u32  meshCount                      equal to the MESH count; mesh i here is MESH record i
per mesh:
    u32  vertexCount
    u32  faceCount
    u32  cornerCount
    f32  positions[3 × vertexCount]
    u32  faceSizes[faceCount]       corners in each face (3 or more)
    u32  corners[cornerCount]       vertex indices, face after face, counter-clockwise seen from the front
    version 2:
    u32  material                   the object's material (into MATL, or NONE)
    u32  faceMaterialCount          0 when no face has its own, otherwise faceCount
    u32  faceMaterials[...]         each face's own material (into MATL, or NONE for the object's)
    version 3:
    u32  uvCount                    0 for none, otherwise cornerCount
    f32  uvs[2 × uvCount]           each corner's UV (u, v), in the same order as corners
    version 4:
    u32  shading                    0 flat, 1 smooth, 2 auto
    f32  smoothAngle                auto: faces meeting at more than this (radians, 0 to π) stay hard
    u32  markCount                  0 when no edge is marked, otherwise cornerCount
    u8   edgeMarks[markCount]       the edge ending at each corner: 0 none, 1 hard, 2 smooth
```

Version 1 files (no materials) still read; their objects get the first part's material. Version 2 files have no UVs; their faces get zeros. Version 3 files are flat with no marks.

The baked normals already carry the shading (a smooth vertex is shared by every face around it, a hard edge splits it), so the engine needs nothing from `EDIT` to draw it.

Re-importing builds the half-edge mesh from these polygons, the same way Valuma builds its presets. A mesh without `EDIT` data (a file from another tool later) is rebuilt from its triangles by joining vertices at the same position; each triangle corner keeps the `uv0` of the vertex it came from, so seams survive the join.

## Reserved section types

Not defined yet. The type codes are kept for these, so nothing else takes them:

| Type | For | Area |
|---|---|---|
| `SKEL` | Skeleton: joints (bones) as a tree, inverse bind matrices | Animation |
| `ANIM` | Animation clips: name, length, keyframes per joint | Animation |
| `AEVT` | Animation events ("footstep" at a time in a clip) | Animation |
| `COLL` | Hitboxes and collision shapes: box, sphere, capsule, convex hull, each named, on a node or a joint (shapes only; what they do is up to the engine) | Hitboxes and collision shapes |
| `ATTP` | Attachment points: name, node or joint, transform | Attachment points |
| `LODS` | Levels of detail: simpler meshes and the distances to switch | Later |

## Reading a file

1. Map or read the file. Check the magic, `version` (refuse anything above what you read), `headerCrc`, and that `fileSize` matches.
2. Check that the directory fits in the file and `directoryCrc` matches.
3. For each entry, check that `offset + storedSize` fits in the file, and skip unknown types and editor-only sections you don't want.
4. Check each section's `crc`. The editor always does. A shipped game may skip this for speed, since CRC checks read every byte.
5. Check that indices stay in range: node parents and meshes, mesh parts and their materials, every material's name, alpha mode, and texture, every texture's format and data range, every vertex and index range against its section's size, and string references against `STRS`. A mesh's index values are below its `vertexCount`. Valuma checks these on import; an engine can check them once when the asset is first imported into its project.
6. Upload `VTXS` and `IDXS`, then read nodes, meshes, and parts in place.

## Writing a file (Valuma)

- Sections are written in the order `STRS`, `NODE`, `MESH`, `PART`, `VTXS`, `IDXS`, `MATL`, `TEXR`, `EDIT`, each padded to 64 bytes. Readers shouldn't rely on that order.
- The file goes to a temporary file first and then replaces any existing one, so a failed export never leaves a damaged asset behind.
- Every reserved field and padding byte is zero, so exporting the same object twice gives the same bytes.

## Code

### The shared library: `shared/vlmobj/`

[vlmobj.hpp](../../shared/vlmobj/vlmobj.hpp), [vlmobj.cpp](../../shared/vlmobj/vlmobj.cpp): the `vlmobj` CMake target, with no dependency on Valuma (only the C++ standard library), so Aevora can link the same code and the two can't drift apart. Everything is in `namespace vlmobj`.

- **Records:** `Header`, `DirectoryEntry`, `StringRef`, `Node`, `Mesh`, `VertexAttribute`, `Part`, `Material` (with `AlphaMode` and `MATERIAL_DOUBLE_SIDED`), `Texture` (with `TextureFormat`), matching the tables above; `static_assert`s pin their sizes and field offsets.
- **Constants:** `MAGIC`, `VERSION`, `NONE`, `Section::*` type codes (including the reserved ones), `Semantic`, `Format` (and `formatSize`), `Compression`, `SECTION_EDITOR_ONLY`, `fourCC`, `crc32`.
- **Node transforms:** `NodeTransform` (translation, quaternion, scale), `nodeTransform(node)`, `combine(parent, local)` (the rule above), and `File::worldTransforms()` (every node's world transform in one pass, since parents come first).
- **`File`** views a buffer in place (it must outlive the `File` and start on an 8-byte boundary; a memory-mapped file or a `std::vector` qualifies). `open(data, size, options)` runs every check in [Reading a file](#reading-a-file) and says why in `error()` when one fails. `ReadOptions` can turn off the checksum pass and the per-index pass. After that: `nodes()`, `meshes()`, `parts()`, `materials()`, `textures()`, `baseColorTexture(material)` (`NONE` for version 1 materials), `textureData(texture)` (spans over the file's records), `vertexData(mesh)` / `indexData(mesh)`, `allVertexData()` / `allIndexData()` (the whole sections, for one upload each), `string(ref)`, `find(type)` and `data(entry)` for other sections, and `readEditData(EditData&)` for the editable polygons.
- **`Writer`:** `addMesh(MeshInput)` (attributes, stride, vertex bytes, `u32` indices, optional parts), `addMaterial(MaterialInput)` (returns its index for parts; `MATL` is written only when there are any; `baseColorTexture` takes an index from `addTexture`), `addTexture(TextureInput)` (name, size, PNG bytes; returns its index; `TEXR` only when there are any), `addNode(NodeInput)` (parents first), `setEditData(EditData)`, then `finish()` returns the file. It interns names into one string table, picks 16- or 32-bit indices, lines data up on 16 bytes and sections on 64, works out bounds from the `F32x3` positions, and fills in every count, offset, and checksum. The same input always gives the same bytes.

The library's own tests are in [shared/vlmobj/tests/](../../shared/vlmobj/tests/) and use nothing from Valuma.

### The reference file

[shared/vlmobj/reference/cube.vlmobj](../../shared/vlmobj/reference/) is Valuma's export of the default cube, checked in. The shared tests read it back (any reader must keep reading it), and Valuma's tests check that exporting the cube today still gives exactly those bytes, so an accidental format change fails a test. After a deliberate change (with the versions bumped), run the tests once with the environment variable `VLMOBJ_WRITE_REFERENCE=1` to rewrite it.

### Valuma's side: `src/asset/`

[asset_file.hpp](../../src/asset/asset_file.hpp) (`namespace AssetFile`, part of `modeling_core`):

| Function | Description |
|---|---|
| `bake(mesh, groupOf)` | Sorts the faces by material group (as `getFaceData` does) and records each group as a `BakedPart`, then writes every face's corners exactly as the viewport draws them (`MeshData::appendFaceCorners`: position, normal flat or smooth by the mesh's shading, UV) and merges vertices whose values are bit-for-bit equal (`-0` counts as `0`). |
| `write(object, materials, textures)` | One object as a file: the baked mesh, one node (no translation, the object's rotation as a quaternion, its scale), its material (from `materials`, Default when it has none; a collection with only Default if not given), the textures its materials use as base color maps (each PNG as stored, from `textures`), and the `EDIT` section (vertices in mesh order, faces as corner lists with their UVs, the exact Euler angles). |
| `write(objects, root, materials, textures)` | An object and everything under it: the root as above (its world rotation and scale), then its children, parents first, each node with its own baked mesh, its transform relative to its parent, and its `EDIT` polygons and angles. Only the materials these objects use are written, in the order they're first used, and only the textures those materials use; each mesh's part points at its object's. |
| `read(bytes, objects, error)` / `read(bytes, objects, materials, error)` / `read(bytes, asset, error)` | Every node as an `ImportedObject` (the object, its parent's place in the list, and its material's place in the file's materials, `NONE` for none), transforms relative to the parent as stored; the second also gives the file's materials as `Material`s; the third fills an `ImportedAsset` (objects, materials, `materialMaps`: each material's texture as a place in `textures` or `NONE`, and the textures, each a `Texture` with no source path whose PNG is checked to be one). |
| `read(bytes, object, error)` | The root node's object: name ("Imported" if empty), rotation (the `EDIT` angles when present, else worked back from the quaternion), scale, position zero. The mesh comes from the editable polygons through `MeshFactory::fromPolygons`, or without them from the triangles, joined where corners share a position exactly (triangles that collapse are dropped). Polygons are checked first (no repeated corner in a face, each directed edge used once, borders that form simple loops) and the result must pass `validate()`, so a bad file is refused, not built into a broken mesh. |
| `save(path, object, error)` / `load(path, object, error)`, the family versions with materials (and textures, for saving), and `load(path, asset, error)` | The same through a file; saving writes `name.vlmobj.exporting` and renames it over the target. |
| `eulerToQuaternion` / `quaternionToEuler` | Between `Transform` angles (X, then Y, then Z) and `x, y, z, w`. Straight up or down (Y at ±90°), X and Z turn about the same axis, so the way back puts it all in Z. |

## In the app

[asset_actions.cpp](../../src/application/actions/asset_actions.cpp), with the naming rules in [export_plan.cpp](../../src/asset/export_plan.cpp).

**Export window** (Ctrl+E, not while a tool runs; a [modal window](application.md#modal-windows)):
- **Folder** with Browse… (Windows' folder picker). It starts at `exportFolder(ctx)`: the folder last exported to (`ctx.project.exportFolder`, saved in the `.vlm`), or `Exports` next to the project file (`Documents\Valuma Studio\Exports` for an unsaved project). Created when exporting if it isn't there.
- **One row per top-level object** (a file holds the object and all its children): checkbox, name, the file it will write, status, and for colliding rows a Replace / Rename / Skip switch. Its children are listed greyed underneath, indented by level, as "in Car.vlmobj". A row starts checked when the object or any of its children is selected (in an edit mode, the object being edited). A header row checks or unchecks everything and sets every colliding row's switch (it shows a choice only when they all agree).
- **Statuses** come from `planExport(items, fileExists)`, worked out every frame (which files exist is checked again at most once a second): **new**, **exists** (a file with that name is in the folder; orange), or **duplicate** (an earlier checked row writes that name; orange, always renamed). File names are `assetFileName(name)`: the object's name with `\ / : * ? " < > |` and control characters made `_`, trailing dots and spaces dropped, `Untitled` if empty, plus `.vlmobj`. Names are compared ignoring case. **Rename** takes the next name that's free both in the folder and in this export (`Rock 2.vlmobj`, `Rock 3.vlmobj`, …); a renamed file name shows in purple. Skipped and unchecked rows claim no name.
- **Footer:** what will happen ("3 files, 1 replacing existing, 1 renamed"), Cancel, and **Export N** (dimmed at 0). Enter exports, Escape closes.
- **Exporting** writes each file with `AssetFile::save` (through a temporary file, with the materials its objects use), remembers the folder in `ctx.project.exportFolder` (not an unsaved change), closes the window, and prints a summary; a file that fails is reported and the others still go.

**Import** (Ctrl+I, Import… in the Objects tab's dropdown and then +, or `import [<path>]`; relative paths start in the export folder): the Open dialog allows several files and starts in the export folder. Each file's root becomes a new object placed like a new preset (`placeNewObject`), named after the file as it's written on disk (made unique); its other nodes become its children with their relative transforms and names (made unique). Each of the file's materials is brought in once with `MaterialCollection::adopt`: a project material with the same name and every value the same is reused; otherwise it's added, renamed if the name is taken ("Granite 2"), and the console says how many were new. One undo step per file. Errors go to the console.

## Changing the format

- **New data:** a new section type (reserved above, or a new code). Older readers skip it.
- **Changing a section's layout:** bump that section's version. Readers keep accepting the old version where they can.
- **Changing the header or directory:** bump the file `version`.
- Update this page in the same change.
