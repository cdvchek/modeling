# Aevora Engine

Developer documentation for Aevora, the game engine and its editor (project files are `.aev`). It is built on the shared code, which has [its own docs](../../shared/docs/README.md): the window, input, console, UI, and OpenGL pieces are described there.

| Document | What it covers |
|---|---|
| [roadmap.md](roadmap.md) | The order of work, what's done, and what the curvature test showed |
| [decisions.md](decisions.md) | The decisions that shape the engine and the game it's for, and what's still open |
| [architecture.md](architecture.md) | How Aevora is laid out and built |
| [project.md](project.md) | Projects: the folder, the `.aev` file format, and the functions that read and write it |
| [jobs.md](jobs.md) | The job system: worker threads, priorities, `parallelFor`, groups, and handing results to the main thread |
| [editor.md](editor.md) | The editor: its frame, screen, file dialogs, keys, and commands |
| [testing.md](testing.md) | What Aevora's tests cover |

## Keeping these up to date

These docs describe the code as it is, except [roadmap.md](roadmap.md) and [decisions.md](decisions.md), which describe what's planned. When a change adds, removes, or changes behavior:

- Update the page for any public function, struct, keybind, or command that changed, and add a page for a new system.
- Mark the step in [roadmap.md](roadmap.md) when it's done.
- Update [architecture.md](architecture.md) if a folder or build target is added.
- A change to shared code belongs in the [shared docs](../../shared/docs/README.md).
