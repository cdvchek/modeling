# Testing

A minimal built-in test runner with no external framework. Each part of the repo has its own tests executable built on it:

| Executable | Covers | Links |
|---|---|---|
| `build/shared_tests.exe` | The shared libraries: math, fonts, UI, input mapping, the console, `.vlmobj`, PNG (below) | Only shared code |
| `build/valuma_tests.exe` | Valuma's mesh, scene, project, and asset code ([its coverage](../../valuma/docs/testing.md)) | `valuma_core` |
| `build/aevora_tests.exe` | Aevora's engine library ([its coverage](../../aevora/docs/testing.md)) | `aevora_engine` |

None covers a renderer, the platform, or tools.

Files: `shared/test/` (the runner), `shared/<library>/tests/`

## Running

```
cmake --build build
build/shared_tests.exe
build/valuma_tests.exe
build/aevora_tests.exe
```

Or the VS Code **Test** task, which runs all three. Each prints `FAIL <test> (file:line): <expression>` for each failed check, then a summary like `41 tests, 247 checks, 0 failures`, and exits non-zero if anything failed.

Shared libraries keep their tests next to them (`shared/<library>/tests/*.cpp`), and those use only shared code, so another program can run them without Valuma.

## Writing a test

[test.hpp](../test/test.hpp) provides two macros:

```cpp
#include "test.hpp"
#include "core/math/vec3.hpp"

TEST_CASE(cross_of_x_and_y_is_z) {
    const Vec3 z = Vec3::cross(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    CHECK(z.z == 1.0f);
}
```

- `TEST_CASE(name)` defines and auto-registers a test. Names must be unique across all files in one executable.
- `CHECK(expr)` records a failure without stopping the test.
- Any new `.cpp` file in a tests folder is picked up automatically (CMake glob); re-run CMake configure after adding one.

## Coverage

| File | Covers |
|---|---|
| `shared/core/tests/font_tests.cpp` | Embedded fonts load every glyph at their own sizes |
| `shared/core/tests/projection_tests.cpp` | `projectToScreen`: center and axis directions, points behind the camera |
| `shared/core/tests/frame_timer_tests.cpp` | `FrameTimer`: zero until the first window ends, averaging over the window, holding the value between updates |
| `shared/ui/tests/ui_context_tests.cpp` | Widgets with fake mouse input: button clicks on release (not when released outside), `wantsMouse` only over regions, a viewport drag crossing the UI stays with the viewport, slider holds on press and tracks outside its region, checkbox toggles on release, `dragFloat3` moves by mouse delta, `dragFloat` is one box across the control, the color picker's square and hue strip set the color as one edit and its hex field commits one edit, HSV and hex conversions round-trip, scoped IDs differ, non-interactive mode, an active widget that disappears is released; panels: header drag keeps the grab offset and stays in bounds, resize from the right edge, left edge stops at the minimum width, corner resize stops at the bounds, top edge resizes instead of moving, shrink to fit smaller bounds, resize cursors; scrolling: wheel scrolls within limits and only over the panel, rows hidden under the header can't be clicked, scrollbar thumb drags; child boxes take the wheel before the panel and their hidden rows can't be clicked; segmented control picks on release; same-label sliders in different scopes edit independently; panel tabs switch on press and still drag the panel; dropdown picks from its list; a click outside an open dropdown only closes it; a tree row dragged past the threshold carries its payload onto another row (and isn't clicked); a tree row's arrow folds without selecting |
| `shared/ui/tests/ui_draw_list_tests.cpp` | UI draw list: vertex/index counts, text snapping and line wrapping, batching rules (fonts, clips), nested clips, `measureText`, `fitText` shortening with dots, color packing |
| `shared/core/tests/screen_drag_tests.cpp` | `screenAngle` is counterclockwise on screen; `wrapAngle` takes the short way across ±π; summed wrapped steps around a circle add up to a full turn |
| `shared/ui/tests/text_input_tests.cpp` | Text field: typing replaces the selected text and Enter commits (one undo step), Escape restores, a click outside commits and is kept from the viewport, editing keys (arrows, Home, Backspace, Delete), an empty edit is dropped, clipboard copy/cut/paste (pasted text cut at a line break), an edit ends when its field isn't drawn; `dragFloat3` click opens typing and a typed value commits, a 2 px wobble is still a click, a non-number is ignored; `ActionMap` keyboard block spares mouse bindings and Quit; `wasActionPressedOrRepeated` follows OS repeats; typed text skips control characters |
| `shared/core/tests/console_tests.cpp` | `Console::getBrowsedIndex` follows Up/Down through the history and back to a fresh command; editing a recalled command stops browsing; a command's printed output becomes one entry under it; an unknown command becomes an error; closing collapses only multi-line entries and later ones stay open; toggling; trailing breaks and empty text; error count and latest error; `CommandSystem::list` is sorted and `execute` reports unknown names; entries a command prints itself appear once, and their echo reaches stdout; a command guard turns a command down by name (it still counts as known) and lets the rest run |
| `shared/ui/tests/radial_tests.cpp` | `RadialLayout::sliceAt` for 8 and 4 slices (up first, clockwise), 5 slices mirror only left/right, dead zone; `sliceDirection` agrees with picking for 2–8 slices; `ringSlice` quad size and local axes |
| `shared/core/tests/action_map_tests.cpp` | `keybindLabel`; `dispatch` runs a handler only on the press frame and in its context, and skips it when `canRun` fails; `isAvailable` and `getKeybind` for bound and unbound actions; modifiers keep S, Ctrl+S, and Ctrl+Shift+S apart (right Ctrl blocks S, Shift alone doesn't); a modal window owns the keys, even over the console; a click that goes down and up within one frame reports both and fires a mouse action; mouse moves add up within a frame and a click at the same spot doesn't change them; a filter that turns an action down stops its key, `dispatch`, `canRun`, and `isAvailable`, and allowing it again restores them; M+F runs only the chord, not F with it, and F alone still runs; `ContextManager` keeps one mode (the earliest of several asked for) and its always-on context, remembers the mode under a tool's context, and without either treats contexts as plain flags; owner contexts and the unblockable action do nothing until set; handlers on one key run in the order of their numbers whatever order they were registered in. These tests use their own small `Action` and `InputContext` lists from `test_input.hpp`, not Valuma's |
| `shared/vlmobj/tests/vlmobj_tests.cpp` | The shared library alone: CRC check value; a written quad reads back exactly (header conventions, 64-byte sections, node, mesh counts, attributes, bounds and sphere, one default part, indices, vertex data, the editor-only `EDIT` section) and writes the same bytes twice; meshes over 65,536 vertices use 32-bit indices; materials round-trip (every field, the double-sided flag, a part pointing at its material) and a part pointing past the materials or an unknown alpha mode is refused; refusals with their messages (magic, newer version, damaged header, cut short, too small, damaged directory, damaged section unless checksums are off, a section past the end, a newer section version, compression, a misaligned buffer); links out of range (node mesh, root not first, name, vertex range, unknown attribute format, an index past the last vertex unless that check is off); unknown sections are skipped; the reference cube reads back; `combine` places a child in a turned, stretched parent and multiplies scales along the child's axes |
| `shared/image/tests/image_tests.cpp` | The image library alone: every grey, RGB, grey + alpha, RGBA, and palette depth decodes to exactly the expected pixels; `tRNS` for grey, RGB, and palettes; stored blocks, and a 300 × 200 picture with dynamic blocks and long back-references; interlaced files refused by name; refusals for a wrong signature, a bad checksum, every truncation, header values PNG doesn't allow, and an image too large; bit flips in the compressed data fail cleanly; a hand-made stored zlib stream, and the size cap; `deflateZlib` output inflates back to the same bytes (empty, tiny, a long run, repeating text, noise, and smooth data longer than the window and one block), with runs and repeats shrinking and noise staying within a few bytes of its size; `encodePng` output decodes back to the same pixels and is far smaller than the pixels for a flat color and for a gradient with a cut-out shape. Fixtures come from `fixtures/make_fixtures.py` (see [image.md](image.md#tests)) |
| `shared/core/tests/color_tests.cpp` | sRGB ↔ linear conversion: endpoints, mid grey, the linear segment near black, a round trip for every 8-bit value |
