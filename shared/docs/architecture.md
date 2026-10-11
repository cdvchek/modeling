# Architecture

Aevora Works is a set of programs for making a game: Valuma Studio (modeling), the Aevora engine, and later Sollaria audio. All of it is from-scratch C++20 on Win32 and OpenGL 3.3. No windowing, UI, or math libraries; GLAD is the only third-party code.

The repo has two halves: `shared/`, code any of the programs can use, and one folder per program (`valuma/`, `aevora/`). Nothing in `shared/` depends on a program. Each program's own layers are in its docs: [Valuma](../../valuma/docs/architecture.md), [Aevora](../../aevora/docs/architecture.md).

## Shared libraries

| Directory | Role | Depends on |
|---|---|---|
| `shared/core/` | Building blocks with no knowledge of any program: math, containers (`DynamicArray`), events, input (key codes, `InputState`, `ActionMap`, `ContextManager`; each program lists its own actions and contexts), console (with the keys that edit its line, `console_input`), bitmap fonts, frame timing, binary reading/writing and CRC-32 (`io/`), `parallelFor` (`thread/`), and the integer and float aliases (`include/types`). The fonts themselves are in `shared/assets/fonts/`. | — |
| `shared/vlmobj/`, `shared/image/` | The `.vlmobj` asset format, and image files (PNG). | — (C++ standard library only) |
| `shared/ui/` | 2D UI draw list in pixel coordinates (shapes, text, clipping), the immediate-mode widget system (`UIContext`), `makeUIInput` (the input system's state as the UI wants it), and the console's panel (`console_view`). No OpenGL. | core (math, fonts, input, console) |
| `shared/gfx/` | OpenGL building blocks for every program: the GLAD loader, the context (`OpenGLContext`), shaders, textures, font atlases, drawing a `UIDrawList`, and draw counters (see [gfx.md](gfx.md#shared-opengl-code)) | core (math, fonts), ui (`UIDrawList`) |
| `shared/platform/` | Win32 window, message pump, key translation, clipboard, file dialogs | core (events, keys) |

**Platform split:** platform-specific code lives in files ending in `_win32.cpp`, all of them in `shared/platform/` and `shared/gfx/` (`opengl_context_win32.cpp`, the WGL context). The programs themselves have none.

## Build targets

The root [CMakeLists.txt](../../CMakeLists.txt) sets the language standards and adds [shared/CMakeLists.txt](../CMakeLists.txt) and each program's own. The shared file defines:

| Target | Contents |
|---|---|
| `vlmobj` (static lib) | The `.vlmobj` format from `shared/vlmobj/`. Depends on nothing else, so the engine can link it too. |
| `image` (static lib) | The PNG reader and writer from `shared/image/` (see [image.md](image.md)). Depends on nothing else. |
| `core` (static lib) | `shared/core/`: math, fonts, input (`InputState`, `ActionMap`, `ContextManager`), the console and command system, frame timing, CRC-32. No OpenGL or Win32. |
| `ui` (static lib) | `shared/ui/`: the draw list and widgets. Links `core`. No OpenGL or Win32. |
| `platform` (static lib) | `shared/platform/`: the Win32 window, message pump, clipboard, and file dialogs. Links `core`, `opengl32`, and `comdlg32` (file dialogs). |
| `gfx` (static lib) | `shared/gfx/`: `glad.c`, the GL context, shaders, textures, fonts, and the UI renderer. Links `core`, `ui`, and `opengl32`. |
| `test_runner` (static lib) | `shared/test/`: `test.hpp` and the runner's `main`. Each tests executable links it. |
| `shared_tests` (exe → `build/shared_tests.exe`) | Every `shared/*/tests/*.cpp`, linked against `core`, `ui`, `vlmobj`, and `image`. |

New `.cpp` files must be added to their library's list in `shared/CMakeLists.txt` by hand. Tests are picked up by glob. Headers are included from `shared/` (`"core/math/vec3.hpp"`, `"ui/ui_context.hpp"`). The tests executable defines `SOURCE_DIR` (the repo root) so tests can find checked-in reference files.

## Conventions

- Integer and float aliases from `shared/core/include/types`: `u8`…`u64`, `i8`…`i64`, `f32`, `f64`, and `INVALID_INDEX`.
- Member variables use `m_` prefix; functions are camelCase; files are snake_case.
- Comments are sparse, one line, and describe a step.
