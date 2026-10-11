# Architecture

Aevora is the game engine and its editor. So far it is a skeleton: a window, the console, workspace tabs, and projects. It is built on the shared Aevora Works code (`shared/`), described in the [shared architecture](../../shared/docs/architecture.md). What it's growing into is in [roadmap.md](roadmap.md).

## Layout

| Folder | Contents |
|---|---|
| `aevora/engine/` | The engine library (`aevora_engine`): what the game and the editor share. No OpenGL or Win32. Now: `project/`, the project file ([project.md](project.md)), and `jobs/`, the job system ([jobs.md](jobs.md)). |
| `aevora/editor/` | The editor program (`bin/aevora.exe`), built on the shared window, input, console, UI, and OpenGL code. `input/` holds its own actions, contexts, and default keys. |
| `aevora/tests/` | Tests for the engine library (`build/aevora_tests.exe`). |

Headers are included from `aevora/` (`"engine/project/project.hpp"`, `"editor/editor.hpp"`) and from `shared/`.

The engine library holds everything the game will need at run time; the editor is that plus tools. The game itself will be a second program on the same library, in its own repo.

## Build targets

Defined in [aevora/CMakeLists.txt](../CMakeLists.txt), on the [shared targets](../../shared/docs/architecture.md#build-targets):

| Target | Contents |
|---|---|
| `aevora_engine` (static lib) | `aevora/engine/`. Links `core`. No OpenGL or Win32, so it can be tested on its own. |
| `aevora` (exe → `bin/aevora.exe`) | `aevora/editor/`. Links `aevora_engine`, `ui`, `platform`, and `gfx`. |
| `aevora_tests` (exe → `build/aevora_tests.exe`) | Every `aevora/tests/*.cpp`, linked against `aevora_engine`. |

New `.cpp` files must be added to their target's list by hand. Tests are picked up by glob.
