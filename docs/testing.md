# Testing

A minimal built-in test runner with no external framework. Tests link against `modeling_core`, so they cover mesh code, math, fonts, UI, and input mapping, but not the renderer, platform, or tools.

Files: `tests/`

## Running

```
cmake --build build
build/tests.exe
```

Or the VS Code **Test** task. It prints `FAIL <test> (file:line): <expression>` for each failed check, then a summary like `41 tests, 247 checks, 0 failures`, and exits non-zero if anything failed.

## Writing a test

[test.hpp](../tests/test.hpp) provides two macros:

```cpp
#include "test.hpp"
#include "mesh_helpers.hpp"

TEST_CASE(split_edge_adds_midpoint) {
    MeshData mesh;
    mesh.setMesh(PresetMesh::Cube);

    // ...
    CHECK(mesh.validate());
}
```

- `TEST_CASE(name)` defines and auto-registers a test. Names must be unique across all files.
- `CHECK(expr)` records a failure without stopping the test.
- Any new `tests/*.cpp` file is picked up automatically (CMake glob); re-run CMake configure after adding one.

[mesh_helpers.hpp](../tests/mesh_helpers.hpp) has shared helpers for building and inspecting meshes.

For mesh operators, always `CHECK(mesh.validate())` after the operation, including after refused operations.

## Coverage

| File | Covers |
|---|---|
| `mesh_ops_tests.cpp` | Cube validity, extrude (repeated, near borders, 5-sided), remove + fill, split, connect, merge (quads, triangles, borders, refusals, repeated), dissolve edge/face and refusals |
| `loop_tests.cpp` | Edge loops, edge rings, face loops on cubes and extrusions, border loops, edge highlight index map |
| `bevel_tests.cpp` | Vertex/edge/face bevel, width clamping, cancel, 4-edge corners, extruded geometry, re-beveling, open meshes (a border edge of an open cube with cancel, a plane corner chamfer, a grid border vertex, an edge running into the border, a grid border edge as a one-sided strip, a face in a grid corner, a vertex on the rim of a hole; each keeps one border loop and the outline in place), beveling in a squashed space (even there, shorter edge limits the width), other vertices stay bit-exact through the space round trip and cancel |
| `font_tests.cpp` | Embedded fonts load every glyph at their own sizes |
| `projection_tests.cpp` | `projectToScreen`: center and axis directions, points behind the camera |
| `frame_timer_tests.cpp` | `FrameTimer`: zero until the first window ends, averaging over the window, holding the value between updates |
| `ui_context_tests.cpp` | Widgets with fake mouse input: button clicks on release (not when released outside), `wantsMouse` only over regions, a viewport drag crossing the UI stays with the viewport, slider holds on press and tracks outside its region, checkbox toggles on release, `dragFloat3` moves by mouse delta, scoped IDs differ, non-interactive mode, an active widget that disappears is released; panels: header drag keeps the grab offset and stays in bounds, resize from the right edge, left edge stops at the minimum width, corner resize stops at the bounds, top edge resizes instead of moving, shrink to fit smaller bounds, resize cursors; scrolling: wheel scrolls within limits and only over the panel, rows hidden under the header can't be clicked, scrollbar thumb drags; child boxes take the wheel before the panel and their hidden rows can't be clicked; segmented control picks on release; same-label sliders in different scopes edit independently; panel tabs switch on press and still drag the panel; dropdown picks from its list; a click outside an open dropdown only closes it |
| `ui_draw_list_tests.cpp` | UI draw list: vertex/index counts, text snapping and line wrapping, batching rules (fonts, clips), nested clips, `measureText`, color packing |
| `preset_tests.cpp` | Every preset: validates, exact element and border counts, normals point outward (or up for flat ones), and stays valid after extrude + inset |
| `face_data_tests.cpp` | GPU face export: layout, outward normals, winding matches normals, per-triangle normals on non-planar faces |
| `object_tests.cpp` | `ObjectCollection` add/get/remove, slot reuse with stale handles, `uniqueName`, copies are independent (undo snapshots); `Selection`: changing the active object clears elements but not lights, `clear()` keeps the active object |
| `screen_drag_tests.cpp` | `screenAngle` is counterclockwise on screen; `wrapAngle` takes the short way across ±π; summed wrapped steps around a circle add up to a full turn |
| `region_tests.cpp` | Region extrude of one face, two adjacent faces (one block, 6 walls), two separate faces, a face on the mesh border; refusals (corner touch, hole, nothing, closed cube) leave the mesh untouched, for inset too; inset of one face is exact, a two-face region gets one 6-quad border and cancels back exactly, inset across a fold stays valid |
| `object_mode_tests.cpp` | Object selection select/deselect/clear (kept when the active object changes); object mode is one selection context that a tool replaces and confirm restores; `rotateEuler` turns points like an axis-angle rotation, including at the gimbal-lock angle; `ObjectSpace` round-trips points, converts world moves to mesh space exactly, and maps world axes onto a rotated mesh |
| `text_input_tests.cpp` | Text field: typing replaces the selected text and Enter commits (one undo step), Escape restores, a click outside commits and is kept from the viewport, editing keys (arrows, Home, Backspace, Delete), an empty edit is dropped, clipboard copy/cut/paste (pasted text cut at a line break), an edit ends when its field isn't drawn; `dragFloat3` click opens typing and a typed value commits, a 2 px wobble is still a click, a non-number is ignored; `ActionMap` keyboard block spares mouse bindings and Quit; `wasActionPressedOrRepeated` follows OS repeats; typed text skips control characters |
| `console_tests.cpp` | `Console::getBrowsedIndex` follows Up/Down through the history and back to a fresh command; editing a recalled command stops browsing; a command's printed output becomes one entry under it; an unknown command becomes an error; closing collapses only multi-line entries and later ones stay open; toggling; trailing breaks and empty text; error count and latest error; `CommandSystem::list` is sorted and `execute` reports unknown names; entries a command prints itself appear once, and their echo reaches stdout |
| `radial_tests.cpp` | `RadialLayout::sliceAt` for 8 and 4 slices (up first, clockwise), 5 slices mirror only left/right, dead zone; `sliceDirection` agrees with picking for 2–8 slices; `ringSlice` quad size and local axes |
| `action_map_tests.cpp` | `keybindLabel`; `dispatch` runs a handler only on the press frame and in its context, and skips it when `canRun` fails; `isAvailable` and `getKeybind` for bound and unbound actions; modifiers keep S, Ctrl+S, and Ctrl+Shift+S apart (right Ctrl blocks S, Shift alone doesn't) |
| `project_file_tests.cpp` | CRC-32 check values; `parallelFor` visits each index once; a mesh with freed slots and a border round-trips packed, with the same positions, face loops, and winding; short or out-of-range mesh data is refused and leaves the mesh alone; a whole project (objects with a removed slot, transforms, a spot light, ambient, camera, active object, every view field) round-trips exactly and saves back to the same bytes; 48 tori take the threaded path both ways; wrong magic, newer format, short file, damaged directory, damaged object, and a chunk past the end are refused with their messages; unknown chunks are skipped; saving twice to a path with a space replaces the file with no temporary left, loads back, and a missing file says so; `History::stateId` follows commit, undo, redo, cancel, and clear |
| `light_tests.cpp` | `LightCollection` add/edit/replace/remove, handle reuse, `handleAt`, ambient clamping, `nextName` |
