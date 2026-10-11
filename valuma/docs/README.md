# Valuma Studio

Developer documentation for Valuma Studio, the modeling application (project files are `.vlm`). It is built on the shared code, which has [its own docs](../../shared/docs/README.md): the window, input, console, UI, and OpenGL pieces are described there.

| Document | What it covers |
|---|---|
| [features.md](features.md) | Roadmap (planned work, gaps, ideas) and everything the app can do today |
| [architecture.md](architecture.md) | How Valuma is layered, the frame loop, and how data flows between systems |
| [application.md](application.md) | Startup, the main loop, and the modal tools (grab, scale, rotate, bevel, extrude) |
| [input.md](input.md) | Valuma's input contexts and the full keybind table |
| [commands.md](commands.md) | Every console command |
| [mesh.md](mesh.md) | Half-edge mesh, generational handles, and every mesh operator |
| [scene.md](scene.md) | Objects, materials, lights, reference images, transforms, camera, selection, picking, and undo/redo history |
| [project.md](project.md) | Saving and opening projects (`.vlm`): what's saved, the binary file format, threads |
| [assets.md](assets.md) | Exporting and importing `.vlmobj` assets: baking, rebuilding, the Export window |
| [renderer.md](renderer.md) | The viewport renderer: its interface, OpenGL backend, shaders, grid, text, debug overlay |
| [testing.md](testing.md) | What Valuma's tests cover |

## Keeping these up to date

These docs describe the code as it is. When a change adds, removes, or changes behavior:

- Update [features.md](features.md): move **Planned** items to **Current** when they ship; add new limitations under **Gaps to close**.
- Update the page for any public function, struct, keybind, command, or shader that changed.
- Update [architecture.md](architecture.md) if a new system is added or a dependency between layers changes.
- A change to shared code belongs in the [shared docs](../../shared/docs/README.md).
