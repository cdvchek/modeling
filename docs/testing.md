# Testing

A minimal built-in test runner with no external framework. Tests link against `modeling_core`, so they cover mesh code, math, and fonts, but not the renderer, platform, or tools.

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
| `bevel_tests.cpp` | Vertex/edge/face bevel, width clamping, cancel, 4-edge corners, extruded geometry, border refusal, re-beveling |
| `font_tests.cpp` | Embedded fonts load every glyph at their own sizes |
| `projection_tests.cpp` | `projectToScreen`: center and axis directions, points behind the camera |
| `frame_timer_tests.cpp` | `FrameTimer`: zero until the first window ends, averaging over the window, holding the value between updates |
| `ui_context_tests.cpp` | Widgets with fake mouse input: button clicks on release (not when released outside), `wantsMouse` only over regions, a viewport drag crossing the UI stays with the viewport, slider holds on press and tracks outside its region, checkbox toggles on release, `dragFloat3` moves by mouse delta, scoped IDs differ, non-interactive mode, an active widget that disappears is released; panels: header drag keeps the grab offset and stays in bounds, resize from the right edge, left edge stops at the minimum width, corner resize stops at the bounds, top edge resizes instead of moving, shrink to fit smaller bounds, resize cursors |
| `ui_draw_list_tests.cpp` | UI draw list: vertex/index counts, text snapping and line wrapping, batching rules (fonts, clips), nested clips, `measureText`, color packing |
| `preset_tests.cpp` | Every preset: validates, exact element and border counts, normals point outward (or up for flat ones), and stays valid after extrude + inset |
| `face_data_tests.cpp` | GPU face export: layout, outward normals, winding matches normals, per-triangle normals on non-planar faces |
| `light_tests.cpp` | `LightCollection` add/edit/replace/remove, handle reuse, `handleAt`, ambient clamping |
