# Shared code

Developer documentation for `shared/`: the code every Aevora Works program is built on. Nothing in it depends on a program. Each program documents itself: [Valuma Studio](../../valuma/docs/README.md) (modeling) and [Aevora Engine](../../aevora/docs/README.md).

| Document | What it covers |
|---|---|
| [architecture.md](architecture.md) | How the repo is laid out, the shared libraries and what each depends on, the build, and conventions |
| [platform.md](platform.md) | Win32 window, OS message pump, key codes, clipboard, file dialogs |
| [events.md](events.md) | Typed event dispatcher that connects the platform layer to a program |
| [input.md](input.md) | Input state, keybinds, actions, and input contexts: the mechanism each program fills with its own lists |
| [console.md](console.md) | The console, its panel, its editing keys, and the command registry |
| [ui.md](ui.md) | 2D UI draw list, the widgets, the UI shader, fonts |
| [gfx.md](gfx.md) | Shared OpenGL code: the context, shaders, textures, font atlases, drawing the UI |
| [math.md](math.md) | Vector and matrix types |
| [image.md](image.md) | The `image` library: reading and writing PNG |
| [vlmobj.md](vlmobj.md) | The `.vlmobj` asset format and the `vlmobj` library that reads and writes it |
| [testing.md](testing.md) | The test runner, and what the shared tests cover |

## Keeping these up to date

These docs describe the code as it is. When shared code changes:

- Update the page for any public function, struct, or shader that changed.
- Update [architecture.md](architecture.md) if a library is added or a dependency between libraries changes.
- If the change also alters what a program does, update that program's docs too.
