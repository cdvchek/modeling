# Documentation

Developer documentation for Valuma Studio, the modeling application (project files are `.vlm`).

| Document | What it covers |
|---|---|
| [features.md](features.md) | Roadmap (planned work, gaps, ideas) and everything the app can do today |
| [architecture.md](architecture.md) | How the project is layered, the frame loop, and how data flows between systems |
| [systems/platform.md](systems/platform.md) | Win32 window, OS message pump, key codes |
| [systems/events.md](systems/events.md) | Typed event dispatcher that connects the platform layer to the app |
| [systems/input.md](systems/input.md) | Input state, actions, keybinds, and input contexts (with the full keybind table) |
| [systems/application.md](systems/application.md) | Startup, the main loop, and the modal tools (grab, scale, rotate, bevel, extrude) |
| [systems/console.md](systems/console.md) | In-app console and the command registry |
| [systems/mesh.md](systems/mesh.md) | Half-edge mesh, generational handles, and every mesh operator |
| [systems/project.md](systems/project.md) | Saving and opening projects (`.vlm`): what's saved, the binary file format, threads |
| [systems/vlmobj.md](systems/vlmobj.md) | The `.vlmobj` asset format exported for the Aevora engine, the shared `vlmobj` library, and Valuma's baking |
| [systems/image.md](systems/image.md) | The shared `image` library: the PNG reader and its DEFLATE decompressor |
| [systems/scene.md](systems/scene.md) | Objects, materials, lights, reference images, transforms, camera, selection, picking, and undo/redo history |
| [systems/renderer.md](systems/renderer.md) | Renderer interface, OpenGL backend, shaders, grid, text, debug overlay |
| [systems/ui.md](systems/ui.md) | 2D UI draw list, UI shader, fonts |
| [systems/math.md](systems/math.md) | Vector and matrix types |
| [testing.md](testing.md) | The test runner and what the tests cover |

## Keeping these up to date

These docs describe the code as it is. When a change adds, removes, or changes behavior:

- Update [features.md](features.md): move **Planned** items to **Current** when they ship; add new limitations under **Gaps to close**.
- Update the system doc for any public function, struct, keybind, command, or shader that changed.
- Update [architecture.md](architecture.md) if a new system is added or a dependency between layers changes.
