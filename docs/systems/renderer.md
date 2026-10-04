# Renderer

Draws the scene through a backend-neutral interface. OpenGL 3.3 is the only backend.

Files: `src/renderer/`, plus [opengl_renderer_win32.cpp](../../src/platform/renderer/opengl_renderer_win32.cpp) for context creation.

| File | Contents |
|---|---|
| [renderer.hpp](../../src/renderer/renderer.hpp) | `IRenderer`, `RendererConfig`, `ClearState`, draw command structs |
| [renderer.cpp](../../src/renderer/renderer.cpp) | `createRenderer(RendererBackend)` |
| [gpu_mesh.hpp](../../src/renderer/gpu_mesh.hpp) | `IMesh` interface |
| [shader.hpp](../../src/renderer/shader.hpp) | `Shader` interface |
| `opengl/opengl_renderer.hpp` | `OpenGLRenderer` class |
| `opengl/opengl_renderer_common.cpp` | Portable GL code: `createResources`/`destroyResources`, and drawing objects, grid, text, debug lines, console |
| `opengl/opengl_mesh.*` | `OpenGLMesh`: GPU buffers for one `MeshData` |
| `opengl/shaders/opengl_shader.*` | `OpenGLShader`: compile, bind, set uniforms |
| `opengl/shaders/opengl_shader_library.*` | `ShaderId`, `OpenGLShaderLibrary`: every program, embedded and compiled together |
| `opengl/shaders/glsl/` | GLSL source files |
| `opengl/opengl_font.*` | `OpenGLFont`: glyph atlas texture from a `BitmapFont` |
| [debug_renderer.hpp](../../src/renderer/debug_renderer.hpp) | Half-edge debug overlay |

## IRenderer

| Method | Description |
|---|---|
| `initialize(window, surface, config)` | Creates the context, then shaders and shared buffers (`createResources`). `window`/`surface` are the native handles from `Window`. |
| `shutdown()` | Destroys the context. |
| `resize(w, h)` / `setVSync(bool)` | Viewport size (also rebuilds the MSAA framebuffer) / swap interval. |
| `beginFrame()` / `beginMainPass(ClearState)` | Start a frame, clear, and draw the gradient background (when `clearColor` is set). The gradient covers the clear color, which only shows if the background shader fails to load. |
| `setBackground(BackgroundGradient)` / `getBackground()` | Top and bottom background colors. Default top 0.24/0.24/0.26, bottom 0.11/0.11/0.12. |
| `setLighting(LightingState)` | Lighting for the following `draw` calls. Call once per frame before drawing objects. |
| `setBackFaceTint(color)` / `getBackFaceTint()` | Color back faces are multiplied by. Default 0.8/0.4/0.4. Set by the `backface tint` command. |
| `draw(DrawCommand)` | Draws one mesh's faces, edges, and vertices. |
| `drawGrid(DrawGridCommand)` | Ground grid and axes. |
| `drawText(DrawTextCommand)` | Screen-space text in pixels (origin top-left). |
| `drawText3D(DrawText3DCommand)` | Text on a quad in world space. |
| `drawDebugLine(start, end, mvp)` | One white line. |
| `drawConsoleBackground()` | Translucent fullscreen overlay. |
| `endMainPass()` / `endFrame()` / `present()` | Resolve the MSAA framebuffer to the window, finish, and swap buffers. |

### Draw commands

```cpp
struct DrawCommand {
    bool showVerts, showEdges, showFaces;
    std::vector<VertexHandle> highlightedVerts;   // drawn yellow
    std::vector<EdgeHandle> highlightedEdges;
    std::vector<FaceHandle> highlightedFaces;
    IMesh* mesh;
    Mat4 model;           // for the normal matrix
    Mat4 mvp;
};

struct DrawGridCommand {
    Mat4 viewProjection;
    Vec3 cameraPosition;
    f32 cameraDistance;    // drives grid spacing level
    f32 farPlane;          // grid fades out approaching this
};
```

```cpp
constexpr u32 MAX_DIRECTIONAL_LIGHTS = 4;

struct LightingState {
    Vec3 ambientColor;      // default white
    f32 ambientStrength;    // default 1

    Vec3 directionalDirections[MAX_DIRECTIONAL_LIGHTS];   // normalized, direction the light travels
    Vec3 directionalColors[MAX_DIRECTIONAL_LIGHTS];       // color × intensity
    u32 directionalCount;
};
```

`LightingState` is backend-neutral and doesn't depend on scene types. `buildLightingState` in [application.cpp](../../src/application/application.cpp) fills it each frame: the ambient light, then the camera headlight in slot 0 when it's on (direction = camera forward, color × strength, from `ctx.viewport.headlight`), then enabled directional lights from `scene.lights` in the remaining slots (3 with the headlight on, 4 with it off). Point and spot lights are ignored for now.

`DrawTextCommand` and `DrawText3DCommand` hold references, so they must be used immediately, not stored.

## OpenGL backend

### Anti-aliasing

Everything between `beginMainPass` and `endMainPass` is drawn into an offscreen multisampled framebuffer (`m_msaaFramebuffer`: RGBA8 color + depth/stencil renderbuffers, 4 samples, capped at `GL_MAX_SAMPLES`). `endMainPass` resolves it into the window with `glBlitFramebuffer`. This is used instead of a multisampled window because the WGL context is created with the basic `ChoosePixelFormat`, which can't request MSAA.

- `createRenderTargets(w, h)` builds it (called from `createResources` and `resize`); `destroyRenderTargets()` frees it.
- If the framebuffer can't be created, an error is printed and drawing goes straight to the window without anti-aliasing.
- The grid already anti-aliases its lines in the shader, so MSAA mainly affects mesh edges, points, and face silhouettes.
- Change `m_msaaSamples` in `opengl_renderer.hpp` for more or fewer samples.

### Object drawing

`OpenGLRenderer::draw` uses the `Lit` shader for faces and `Unlit` for everything else:
1. Highlighted faces in yellow (`Unlit`, so selection stays bright), then all faces with `Lit`, with polygon offset so edges draw on top. `Lit` computes `0.7 gray × (ambientColor × ambientStrength + Σ directionalColor × max(dot(normal, −direction), 0))`. Normals come from the face buffer, transformed by `transpose(inverse(model))`. Back faces (seen through holes in open meshes, or a face whose normal got flipped) are lit with the normal flipped, then their base color is multiplied by a muted red tint (`u_BackFaceTint`, from `setBackFaceTint`, default 0.8/0.4/0.4) so they're recognizable at a glance. On a closed mesh, a red face means its winding is wrong.
2. Highlighted edges in yellow, then all edges in dark gray, 2 px wide.
3. Highlighted vertices in yellow, then all vertices as 8 px near-black points.

Colors are constants at the top of `opengl_renderer_common.cpp`: `FACE_COLOR` (0.7), `EDGE_COLOR` (0.2), `VERTEX_COLOR` (0.1), `SELECTED_COLOR` (yellow).

Faces are flat-shaded: one normal per face. See [features.md](../features.md#lighting-and-look) for what's next.

### Shaders

Everything shader-related is in `src/renderer/opengl/shaders/`; the platform layer doesn't touch shaders.

GLSL lives in real files under `glsl/` and is compiled into the executable with `#embed` in [opengl_shader_library.cpp](../../src/renderer/opengl/shaders/opengl_shader_library.cpp) (paths are relative to that file). Editing a `.vert`/`.frag` file rebuilds it automatically. Keep GLSL files ASCII.

Programs are looked up by `ShaderId` through `OpenGLShaderLibrary`:

| `ShaderId` | Vertex | Fragment | Used for | Uniforms |
|---|---|---|---|---|
| `Unlit` | `unlit.vert` | `unlit.frag` | Mesh edges, vertices, highlights, debug lines | `u_MVP`, `u_Color` |
| `Lit` | `lit.vert` | `lit.frag` | Mesh faces | `u_MVP`, `u_NormalMatrix`, `u_Color`, `u_BackFaceTint`, `u_AmbientColor`, `u_AmbientStrength`, `u_DirectionalLightDirections[4]`, `u_DirectionalLightColors[4]`, `u_DirectionalLightCount` |
| `ScreenText` | `screen_text.vert` | `text.frag` | Console text, positions already in NDC | `u_Color`, `u_Texture` |
| `WorldText` | `world_text.vert` | `text.frag` | Debug labels in world space | `u_MVP`, `u_Color`, `u_Texture` |
| `ConsoleBackground` | `fullscreen.vert` | `console_background.frag` | Translucent console overlay | — |
| `Background` | `fullscreen.vert` | `background.frag` | Viewport gradient, drawn first with depth test and writes off. Blends `u_BottomColor` → `u_TopColor` by `gl_FragCoord.y / u_ViewportHeight` and adds ±½/255 noise to break up 8-bit banding. | `u_TopColor`, `u_BottomColor`, `u_ViewportHeight` |
| `Grid` | `grid.vert` | `grid.frag` | Ground grid and axes | see [Grid](#grid) |

Naming in GLSL: `a_` vertex inputs, `v_` values passed to the fragment stage, `u_` uniforms. `fullscreen.vert` and `grid.vert` take positions from `gl_VertexID` and are drawn with the empty `m_fullscreenVAO`.

`OpenGLShader` methods: `create(name, vert, frag)`, `destroy()`, `bind()`, `setMat4`, `setVec3`, `setFloat`, `setInt` (use for samplers). Uniform locations are cached per name. Matrices are uploaded column-major without transposing. Compile and link errors are printed to stderr with the shader's name. A program that fails to load is skipped: `bind()` returns false and the draws that use it return early. The rest of the app keeps running.

**Adding a shader:** write the `.vert`/`.frag` in `glsl/`, add an `#embed` array for each new file and a `ShaderId` entry plus a row in `SOURCES` in `opengl_shader_library.cpp` (a `static_assert` catches a missing row), then `m_shaders.get(ShaderId::...)` in the draw code. `MAX_DIRECTIONAL_LIGHTS` in `renderer.hpp` and `lit.frag` must match.

### Grid

`drawGrid` in `opengl_renderer_common.cpp`; shaders `glsl/grid.vert` / `glsl/grid.frag`.

- **Geometry:** a single fullscreen triangle with no vertex buffer (`m_fullscreenVAO` is empty; positions come from `gl_VertexID`). The vertex shader unprojects each corner to near- and far-plane world points.
- **Plane:** the fragment shader intersects each pixel's view ray with `y = 0` and discards pixels whose ray misses (above the horizon or beyond the far plane). It writes `gl_FragDepth` from the hit point so meshes hide the grid correctly.
- **Spacing levels:** chosen on the CPU from `cameraDistance`:
  ```
  level   = max(0, log_levelFactor(cameraDistance / baseDistance))
  spacing = baseSpacing × levelFactor ^ floor(level)
  blend   = fract(level)
  ```
  The shader draws lines at `spacing` (thin, fading out with `blend`), `5 × spacing` (thick, thinning as `blend` → 1), and `25 × spacing` (thick, fading in). At `blend = 1` this matches the next level at `blend = 0`, so zooming never pops.
- **Line quality:** lines are a fixed pixel width using `fwidth`, and fade out when the grid cells get smaller than a few pixels.
- **Axes:** the X axis (`z = 0`) is drawn red and the Z axis (`x = 0`) blue, 2 px wide.
- **Fade:** alpha falls off between 50% and 100% of `farPlane` from the camera.
- **State:** drawn after objects with blending on and depth writes off; both are restored afterward.

Tunables:

| Where | Name | Default | Effect |
|---|---|---|---|
| `drawGrid` | `baseSpacing` | 0.5 | Finest line spacing |
| `drawGrid` | `levelFactor` | 5 | Each level is this many times coarser. If changed, also change the `5.0` and `25.0` multipliers in `grid.frag`. |
| `drawGrid` | `baseDistance` | 2.0 | Camera distance where the second level starts fading in. Level *n* is fully reached at `baseDistance × levelFactor^n`. |
| `grid.frag` | `MINOR_ALPHA`, `MAJOR_ALPHA`, `AXIS_ALPHA` | 0.18, 0.4, 0.9 | Line opacity |
| `grid.frag` | `LINE_COLOR`, `X_AXIS_COLOR`, `Z_AXIS_COLOR` | gray, red, blue | Colors |

### GPU meshes

`OpenGLMesh` ([opengl_mesh.hpp](../../src/renderer/opengl/opengl_mesh.hpp)) implements `IMesh` and owns two VAOs:
- **Points and edges:** shared positions (`getVertexData`) and the edge index buffer. Attribute 0 = position.
- **Faces:** the per-face corner buffer from `getFaceData` and its index buffer. Attribute 0 = position, 1 = normal.

| Method | Description |
|---|---|
| `create(meshData)` / `update(meshData)` | Rebuild all buffers from `MeshData::getVertexData/getEdgeData/getFaceData`. `update` calls `create` the first time. |
| `drawVertices()` / `drawEdges()` / `drawFaces()` | Draw everything. |
| `drawVertex(h)` / `drawEdge(h)` / `drawFace(h)` | Draw one element, using the index maps from the export to find its range. Used for highlights. |
| `destroy()` | Free GL objects. |

Normals are computed on the CPU during export, so they're only recalculated when the mesh is marked dirty, not every frame.

### Text

`BitmapFont` ([bitmap_font.hpp](../../src/core/font/bitmap_font.hpp)) loads a `.bmf` file of 16×24 1-bit glyphs for ASCII 32–126. The console font is compiled into the binary with `#embed` ([embedded_fonts.cpp](../../src/core/font/embedded_fonts.cpp); assets path set by `--embed-dir` in CMake). `OpenGLFont` uploads the glyphs to a texture atlas and returns `GlyphUV`s. `drawText` builds one quad per character in a dynamic buffer; `\n` starts a new line.

`tools/fix_bmf_baseline.py` adjusts glyph baselines in a `.bmf` file.

## Debug renderer

`DebugRenderer::render(renderer, scene, viewProjection)` runs when the `Debug` context is on (console command `debug`). For every half-edge of every object it draws an arrow, offset toward its face (or away, for border edges) so the two halves of an edge are both visible, plus an `index:generation` label using `drawText3D`.

## Adding a backend

Implement `IRenderer` and `IMesh`, add a case to `createRenderer`. `Object` currently stores `OpenGLMesh` directly, so that would need to become backend-neutral first.
