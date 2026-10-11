# Assets (.vlmobj)

How Valuma writes and reads `.vlmobj` assets: baking an object into a file, rebuilding one from it, and the Export and Import windows. The format itself, and the library that reads and writes it, are shared: see the [format page](../../shared/docs/vlmobj.md).

Files: `valuma/src/asset/`, [asset_actions.cpp](../src/application/actions/asset_actions.cpp)

## Writing a file

- Sections are written in the order `STRS`, `NODE`, `MESH`, `PART`, `VTXS`, `IDXS`, `MATL`, `TEXR`, `EDIT`, each padded to 64 bytes. Readers shouldn't rely on that order.
- The file goes to a temporary file first and then replaces any existing one, so a failed export never leaves a damaged asset behind.
- Every reserved field and padding byte is zero, so exporting the same object twice gives the same bytes.

## Baking and rebuilding

[asset_file.hpp](../src/asset/asset_file.hpp) (`namespace AssetFile`, part of `valuma_core`):

| Function | Description |
|---|---|
| `bake(mesh, groupOf)` | Sorts the faces by material group (as `getFaceData` does) and records each group as a `BakedPart`, then writes every face's corners exactly as the viewport draws them (`MeshData::appendFaceCorners`: position, normal flat or smooth by the mesh's shading, UV) and merges vertices whose values are bit-for-bit equal (`-0` counts as `0`). |
| `write(object, materials, textures)` | One object as a file: the baked mesh, one node (no translation, the object's rotation as a quaternion, its scale), its material (from `materials`, Default when it has none; a collection with only Default if not given), the textures its materials use as base color maps (from `textures`: a texture's PNG as stored, or for a layered texture its combined picture from `flattenedPng`), and the `EDIT` section (vertices in mesh order, faces as corner lists with their UVs, the exact Euler angles). |
| `write(objects, root, materials, textures)` | An object and everything under it: the root as above (its world rotation and scale), then its children, parents first, each node with its own baked mesh, its transform relative to its parent, and its `EDIT` polygons and angles. Only the materials these objects use are written, in the order they're first used, and only the textures those materials use; each mesh's part points at its object's. |
| `read(bytes, objects, error)` / `read(bytes, objects, materials, error)` / `read(bytes, asset, error)` | Every node as an `ImportedObject` (the object, its parent's place in the list, and its material's place in the file's materials, `NONE` for none), transforms relative to the parent as stored; the second also gives the file's materials as `Material`s; the third fills an `ImportedAsset` (objects, materials, `materialMaps`: each material's texture as a place in `textures` or `NONE`, and the textures, each a `Texture` with no source path whose PNG is checked to be one). |
| `read(bytes, object, error)` | The root node's object: name ("Imported" if empty), rotation (the `EDIT` angles when present, else worked back from the quaternion), scale, position zero. The mesh comes from the editable polygons through `MeshFactory::fromPolygons`, or without them from the triangles, joined where corners share a position exactly (triangles that collapse are dropped). Polygons are checked first (no repeated corner in a face, each directed edge used once, borders that form simple loops) and the result must pass `validate()`, so a bad file is refused, not built into a broken mesh. |
| `save(path, object, error)` / `load(path, object, error)`, the family versions with materials (and textures, for saving), and `load(path, asset, error)` | The same through a file; saving writes `name.vlmobj.exporting` and renames it over the target. |
| `eulerToQuaternion` / `quaternionToEuler` | Between `Transform` angles (X, then Y, then Z) and `x, y, z, w`. Straight up or down (Y at ±90°), X and Z turn about the same axis, so the way back puts it all in Z. |

## In the app

[asset_actions.cpp](../src/application/actions/asset_actions.cpp), with the naming rules in [export_plan.cpp](../src/asset/export_plan.cpp).

**Export window** (Ctrl+E, not while a tool runs; a [modal window](application.md#modal-windows)):
- **Folder** with Browse… (Windows' folder picker). It starts at `exportFolder(ctx)`: the folder last exported to (`ctx.project.exportFolder`, saved in the `.vlm`), or `Exports` next to the project file (`Documents\Valuma Studio\Exports` for an unsaved project). Created when exporting if it isn't there.
- **One row per top-level object** (a file holds the object and all its children): checkbox, name, the file it will write, status, and for colliding rows a Replace / Rename / Skip switch. Its children are listed greyed underneath, indented by level, as "in Car.vlmobj". A row starts checked when the object or any of its children is selected (in an edit mode, the object being edited). A header row checks or unchecks everything and sets every colliding row's switch (it shows a choice only when they all agree).
- **Statuses** come from `planExport(items, fileExists)`, worked out every frame (which files exist is checked again at most once a second): **new**, **exists** (a file with that name is in the folder; orange), or **duplicate** (an earlier checked row writes that name; orange, always renamed). File names are `assetFileName(name)`: the object's name with `\ / : * ? " < > |` and control characters made `_`, trailing dots and spaces dropped, `Untitled` if empty, plus `.vlmobj`. Names are compared ignoring case. **Rename** takes the next name that's free both in the folder and in this export (`Rock 2.vlmobj`, `Rock 3.vlmobj`, …); a renamed file name shows in purple. Skipped and unchecked rows claim no name.
- **Footer:** what will happen ("3 files, 1 replacing existing, 1 renamed"), Cancel, and **Export N** (dimmed at 0). Enter exports, Escape closes.
- **Exporting** writes each file with `AssetFile::save` (through a temporary file, with the materials its objects use), remembers the folder in `ctx.project.exportFolder` (not an unsaved change), closes the window, and prints a summary; a file that fails is reported and the others still go.

**Import** (Ctrl+I, Import… in the Objects tab's dropdown and then +, or `import [<path>]`; relative paths start in the export folder): the Open dialog allows several files and starts in the export folder. Each file's root becomes a new object placed like a new preset (`placeNewObject`), named after the file as it's written on disk (made unique); its other nodes become its children with their relative transforms and names (made unique). Each of the file's materials is brought in once with `MaterialCollection::adopt`: a project material with the same name and every value the same is reused; otherwise it's added, renamed if the name is taken ("Granite 2"), and the console says how many were new. One undo step per file. Errors go to the console.
