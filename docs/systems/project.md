# Project files

Saving and opening Valuma Studio projects (`.vlm`). Opening a file puts the app back where it was when you saved, so you can keep working on the scene.

Files: [valuma/src/project/](../../valuma/src/project/) (the file format, in `valuma_core`), [project_actions.cpp](../../valuma/src/application/actions/project_actions.cpp) (save, open, new, the title bar, and the unsaved-changes prompt), and [project_state.hpp](../../valuma/src/application/project_state.hpp).

## What's saved

| Saved | Not saved |
|---|---|
| Objects: name, transform, material, and the whole half-edge mesh | The selection (elements, objects, lights, images) |
| Materials, every value, Default included | Which material the Materials tab shows |
| Lights, every property, and the ambient light | Undo history: opening, or starting a new project, starts it fresh |
| Reference images, every setting, with their PNG files | |
| Camera: position, target, up, orbit distance/yaw/pitch, field of view, near/far planes | Console history and vsync (app settings, not project) |
| The active object | A running tool: saving or opening is refused until it's confirmed or cancelled |
| Selection mode, and the edit mode Tab returns to | |
| Headlight, exposure, material or clay view, back-face tint, debug overlay on/off, origins shown | |
| The panel: shown or hidden, position and size, open tab | |
| The export folder (relative to the project file when nearby) | |

## Using it

| Key | Console | Does |
|---|---|---|
| Ctrl+S | `save` | Saves to the project's file; the first time, asks where (like Save As). |
| Ctrl+Shift+S | `save <path>` | Save As: the dialog, or the given path. |
| Ctrl+O | `open` / `open <path>` | Opens a project. |
| Ctrl+N | `new` | Starts over with the default scene (a cube and the Sun light) and the startup view settings. The panel stays where it is. |
| — | `fileinfo [<path>]` | Lists a file's sections, with sizes and checksums (the current project's file if no path). |

- **Where projects go:** `Documents\Valuma Studio` (`projectsFolder()`, created when first needed; Documents is found with `SHGetKnownFolderPath`, so it follows OneDrive redirection, and if it can't be found the working folder is used). Save As and Open start there for a project that hasn't been saved; once it has a file, they start in that file's folder. (Windows has its own remembered-folder rule that can start a dialog in the folder you last browsed to instead.)
- Console paths: `.vlm` is added when there's no extension; a relative path is taken from `Documents\Valuma Studio` (`save scene` writes `Documents\Valuma Studio\scene.vlm`, `save props\crate` a file in a `props` folder there, which must exist); an absolute path is used as is. Spaces are fine (surrounding quotes are dropped). Type `\` between folders, since `/` closes the console.
- Saving and opening print the file name and how long it took to the console. Errors print there too and flash in the status bar.
- The window title shows `name.vlm - Valuma Studio` (or `Untitled`), with a `*` after the name when there are unsaved changes.
- **Unsaved changes** are anything undoable: `History::stateId()` differs from the id saved at the last save, open, or new. Undoing back to the saved state clears the `*`. Moving the camera or the panel, switching modes, or the view toggles are saved with the file but don't count as changes.
- Before New, Open, or closing the window with unsaved changes, a prompt in the app asks **Save changes to name?** (see [Modal windows](application.md#modal-windows)). **Save** saves (asking where if the project has no file yet) and carries on only if that worked, **Don't Save** throws the changes away, **Cancel** (Escape) stops what was about to happen. `confirmDiscardChanges(ctx, then)` runs `then` right away when nothing is unsaved, otherwise after the answer, on the next frame.
- Opening reads into a separate `Scene` and only replaces the current one if everything checks out, so a damaged file leaves your work alone. Opening then clears undo history, sets the view, and switches to the saved mode (object mode selects the active object, as usual).

The keys are bound in Selection contexts, so they don't fire while the console is open (use the commands) or while a text field has the keyboard. A dialog takes the key-ups for keys held when it opened, so after one the app forgets every held key (`InputState::releaseAll`).

## File format

All numbers are little-endian. A file is a header, a directory with one entry per chunk, then the chunks, each starting on an 8-byte boundary (zero padding between them).

**Header** (16 bytes)

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | Magic `VLMS` |
| 4 | `u32` | Format version (`ProjectFile::FORMAT_VERSION`, now 1) |
| 8 | `u32` | Chunk count |
| 12 | `u32` | CRC-32 of the directory |

**Directory entry** (32 bytes each, right after the header)

| Offset | Type | Field |
|---|---|---|
| 0 | `u32` | Chunk type: four characters, first character in the low byte, so it reads as text in a hex dump |
| 4 | `u32` | Chunk version |
| 8 | `u64` | Offset of the chunk from the start of the file |
| 16 | `u64` | Size in bytes |
| 24 | `u32` | CRC-32 of the chunk's bytes |
| 28 | `u32` | Reserved (0) |

**Chunks** (version 1 unless noted). Strings are a `u32` length then the characters (UTF-8). `bool` is one byte. `vec3` is three `f32`.

| Type | Contents |
|---|---|
| `VIEW` (version 8) | `u32` selection mode (0 vertex, 1 edge, 2 face, 3 object), `u32` last edit mode (0–2), `u32` active object (its place among the `OBJC` chunks, or `0xFFFFFFFF` for none), `bool` debug, `bool` headlight on, `vec3` headlight color, `f32` headlight strength, `vec3` back-face tint, `bool` panel shown, `f32 ×4` panel rect (x, y, width, height), `i32` panel tab; version 2 adds `bool` origins shown (version 1 files read as shown); version 3 adds `f32` exposure in stops and `bool` material view (older files read 0 and material view); version 4 adds `bool` UV checker shown (older files read off); version 5 adds `u8` workspace (0 Model, 1 UV, 2 Paint) and `f32` the UV workspace's split (0 to 1; older files open in Model at the middle); version 6 adds the UV editor's view: `f32 ×2` center (UV), `f32` zoom (pixels per UV unit; 0 frames everything when first shown), `bool` grid (older files frame everything with the grid on); version 7 adds Paint's view: `bool` 2D, `f32 ×2` the 2D view's center, `f32` its zoom (older files open Paint in 3D and frame the texture); version 8 adds Paint's brush: `vec3` color, `f32` size in texture pixels, `f32` softness, `f32` opacity, `f32` spacing (a part of the size), `bool` eraser (older files get the default brush; the app clamps whatever it reads into range). The texture being painted isn't saved (the first one the object uses is picked). The editor's background choice isn't saved: it goes back to the material's |
| `CAMR` | `vec3` position, target, up; `f32` distance, yaw, pitch, field of view (radians), near, far |
| `LITE` | `vec3` ambient color, `f32` ambient strength, `u32` light count, then per light: string name, `u32` type (0 point, 1 directional, 2 spot), `vec3` position, direction, color, `f32` intensity, range, inner cone, outer cone, `bool` enabled |
| `EXPT` | String: the export folder as `storeFolder` gives it, empty for the default (`Exports` next to the project). Files without this chunk get the default. |
| `REFI` | One per reference image, in order: string name, `vec3` position, rotation, `f32` size, opacity, `u8` depth (0 behind, 1 scene, 2 front), `bool` locked, visible, string file name, `u32` width, height (pixels), `u64` PNG size, then the PNG file's bytes unchanged. Opening checks the sizes and that the bytes start like a PNG, but doesn't decode the picture: that happens when it's first drawn, which reports a damaged picture in the console. |
| `MATL` | Every material, Default first: `u32` count, then per material: string name, `vec3` base color (sRGB), `f32` roughness, metallic, `vec3` emissive color (sRGB), `f32` emissive strength, opacity, `u8` alpha mode (0 opaque, 1 cutout, 2 blend), `f32` alpha cutoff, `bool` double-sided, then (version 2) `u32` base color map (its texture's place among the `TEXR` chunks, `0xFFFFFFFF` for none; linked after every chunk is read, so the order doesn't matter, and refused if there's no such texture). The first fills in Default. Files without this chunk have only Default. |
| `TEXR` | One per texture, in order: string name, string source path (the file it was loaded from, UTF-8; empty for one from an asset), then (version 2) `bool` layered. **Layered:** `u32` width, height, layer count (at least 1), active layer; then each layer, bottom first: string name, `bool` visible, `f32` opacity (0 to 1), `bool` from file (the layer Reload replaces), `u64` PNG size, and the layer's own pixels as a PNG of the texture's size. Layers that changed since they were last saved or loaded are encoded when saving, each on its own thread; the rest are written from the PNG they already have, so saving a loaded project writes the same bytes. On opening, every layer's PNG is decoded (each on its own thread) and one that isn't the texture's size is refused. **Not layered** (and always in version 1): the picture as in `REFI`: string file name, `u32` width, height, `u64` PNG size, the PNG file's bytes unchanged. Checked the same way, decoded when first drawn. |
| `OBJC` (version 7) | One per object, in object order: string name, `vec3` position, rotation, scale (relative to the parent), `u32` parent (its place among the `OBJC` chunks, or `0xFFFFFFFF`; added in version 2, version 1 objects have none), `u32` material (its place in `MATL`, 0 for Default; added in version 3, older objects use Default), then the mesh (below), then (version 4) `u32` face count and that many `u32` (each face's own material as its place in `MATL`, `0xFFFFFFFF` for none, in the mesh's face order), or a count of 0 when no face has one, then (version 5) `u32` half-edge count and that many `f32 ×2` UVs (`getCornerUVs`: each half-edge's UV in the mesh's half-edge order, border ones included); older objects get zeros; then (version 6) `u8` shading (0 flat, 1 smooth, 2 auto), `f32` auto angle in radians (0 to π), `u32` mark count (0 when nothing is marked, otherwise the half-edge count) and that many `u8` marks (0 none, 1 hard, 2 smooth) in the same order; older objects are flat; then (version 7) `u32` seam count (0 when there are no seams, otherwise the half-edge count) and that many `u8` (1 a UV seam, 0 not), in the same order. Opening checks that every parent exists and that no chain of parents loops back, and that every material exists (and the face list matches the faces, the UV list the half-edges), and links them as stored. |

**Mesh** (from `MeshData::writeTo`, see [mesh.md](mesh.md#files)): `u32` vertex, half-edge, and face counts; `f32[3 × vertices]` positions; `u32[vertices]` each vertex's outgoing half-edge; `u32[5 × half-edges]` each half-edge's tip, pair, next, prev, face; `u32[faces]` each face's half-edge. Links are indices into these arrays; `0xFFFFFFFF` means none (a border half-edge's face, a lone vertex's edge). Deleted slots are packed out when saving, so the arrays have no gaps, and the arrays are read straight into memory.

### Reading rules

- Wrong magic: "not a Valuma Studio project". A higher format version: refused as made by a newer version.
- The directory's CRC must match, and every chunk must lie inside the file.
- **Unknown chunk types are skipped**, so a file from a newer version with extra sections (materials, textures, …) still opens. A known chunk with a higher version than this build reads is refused.
- Each chunk's CRC is checked before it's read; a mismatch names the chunk ("chunk OBJC is damaged").
- Every mesh link is range-checked before any handle is built, and each mesh must pass `MeshData::validate()`.
- A missing chunk keeps its defaults (for `VIEW`, the current view).

### Threads

Objects are independent chunks, so both directions split them across threads (`parallelFor` in [parallel_for.hpp](../../shared/core/thread/parallel_for.hpp): one thread per core, the caller included, handing out one object at a time so a huge mesh next to small ones still balances).

- **Saving:** each object is encoded into its own buffer (and its CRC computed) in parallel, then the offsets are worked out and the buffers written in order.
- **Opening:** the whole file is read in one go; each object's chunk is checked, decoded, validated, and triangulated (filling the face triangle cache) on a worker, each into its own slot. The objects are then added to the scene in file order on the main thread. GPU buffers are built on the main thread on the next frame, since OpenGL is single-threaded.
- Small projects stay on one thread: saving goes parallel at 2+ objects with 20,000+ half-edges in total, opening at 2+ objects with 1 MB+ of object data (`PARALLEL_SAVE_EDGES`, `PARALLEL_LOAD_BYTES`), since starting threads costs more than it saves below that.

### Safe saving

`ProjectFile::save` writes to `name.vlm.saving` next to the target, then renames it over the old file. If writing fails (no permission, disk full), the temporary file is removed and the old file is untouched.

## API

[project_file.hpp](../../valuma/src/project/project_file.hpp)

| Function | Description |
|---|---|
| `write(scene, view)` | The whole file as bytes. |
| `read(bytes, scene, view, error)` | Fills an empty `Scene` (objects, materials, lights, reference images, camera, active object) and a `View`; on failure, `error` says why. |
| `save(path, scene, view, error)` / `load(path, scene, view, error)` | The same, to and from disk, with the safe save. |
| `readFile(path, bytes, error)` | Reads a whole file. |
| `storeFolder(folder, projectFile)` / `resolveFolder(stored, projectFile)` | A folder as the project stores it: relative to the project's folder when it's inside it or at most one level up (`Exports`, `../Shared`), so moving the project together with it keeps it working; otherwise, on another drive, or with no project file, the full path. UTF-8 with forward slashes. And back to a full path for wherever the project is now. |
| `describe(bytes)` | A readable listing: header, then each chunk's type, version, offset, size, CRC (`DAMAGED` when it doesn't match), and for objects the name, counts, and material, for materials how many, for reference images the name, file, and size. |

`ProjectFile::View` is the editor state saved with the scene (mode, headlight, tint, debug, panel, export folder). The app fills it from `AppContext` and applies it after opening (`captureView` / `applyView` in project_actions.cpp).

Helpers in core: [binary_io.hpp](../../shared/core/io/binary_io.hpp) (`BinaryWriter` appends plain values, arrays, and strings to a buffer; `BinaryReader` reads them back and fails, for good, on any read past the end) and [crc32.hpp](../../shared/core/io/crc32.hpp) (standard CRC-32, eight bytes per step).

## Adding to the format

- **New data:** add a chunk type with `fourCC("NAME")`, add it to `CHUNK_VERSIONS`, write it in `buildChunks`, and read it in `read`. Older builds skip it.
- **Changing a chunk:** bump its version in `CHUNK_VERSIONS` and keep reading the old version (switch on `entry.version`). Older builds will refuse the new version.
- **Changing the header or directory layout:** bump `FORMAT_VERSION`.
- Update this page's tables.
