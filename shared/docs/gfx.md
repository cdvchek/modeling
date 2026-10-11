# Shared OpenGL code

Files: `shared/gfx/`

`shared/gfx/` is the `gfx` library: the pieces of an OpenGL renderer that don't depend on what a program draws. It links `core` and `ui` and knows nothing about Valuma. Everything but the context needs a context to be current.

| File | Contents |
|---|---|
| `glad/`, `KHR/` | The GLAD loader (`#include <glad/glad.h>`) |
| [opengl_context.hpp](../gfx/opengl/opengl_context.hpp), `opengl_context_win32.cpp` | `OpenGLContext`: `create(surface)` picks a pixel format on the window's display context, creates a legacy (compatibility) context with `wglCreateContext`, makes it current, and loads GL with GLAD, returning false and leaving nothing behind if any step fails; `destroy()`; `setVSync(bool)` (`wglSwapIntervalEXT`); `present()` swaps the buffers. The header has no Win32 types. |
| [shader.hpp](../gfx/shader.hpp), `opengl/opengl_shader.*` | The `Shader` interface and `OpenGLShader`: compile, bind, set uniforms |
| [opengl_texture.hpp](../gfx/opengl/opengl_texture.hpp) | `namespace OpenGLTexture`: `create(pixels, width, height)`, `destroy`, `update` (a block of pixels), `refreshMipmaps`, for RGBA8 textures |
| `opengl/opengl_font.*` | `OpenGLFont`: glyph atlas texture from a `BitmapFont`, one per `FontId` |
| `opengl/opengl_ui_renderer.*`, `opengl/glsl/ui.vert`, `ui.frag` | `OpenGLUIRenderer`: draws a `UIDrawList` with its own shader, embedded with `#embed` (see [ui.md](ui.md)). `create()` returns false if the shader failed, and `draw(list, fonts, width, height)` then does nothing. |
| [render_stats.hpp](../gfx/render_stats.hpp), `opengl/opengl_counters.hpp` | `RenderStats`, and the `drawElements`/`drawArrays` wrappers that count every draw into one global set (`glCounters()`) |

A program's renderer owns an `OpenGLContext`, its own shaders (as `OpenGLShader`s) and scene drawing, an `OpenGLFont` per font, and an `OpenGLUIRenderer`. Valuma's `OpenGLRenderer` is one such renderer.

## OpenGLShader

`OpenGLShader` methods: `create(name, vert, frag)`, `destroy()`, `bind()`, `setMat4`, `setVec3`, `setVec3Array`, `setFloat`, `setFloatArray`, `setInt` (use for samplers). Array setters take the name of element 0, e.g. `"u_Lights[0]"`. Uniform locations are cached per name. Matrices are uploaded column-major without transposing. Compile and link errors are printed to stderr with the shader's name. A program that fails to load is skipped: `bind()` returns false and the draws that use it return early. The rest of the app keeps running.
