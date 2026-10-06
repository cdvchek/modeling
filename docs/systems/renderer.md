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
| `opengl/opengl_renderer_common.cpp` | Portable GL code: `createResources`/`destroyResources`, and drawing objects, grid, world text, debug lines |
| `opengl/opengl_ui_renderer.*` | `OpenGLUIRenderer`: draws a `UIDrawList` (see [ui.md](ui.md)) |
| `opengl/opengl_mesh.*` | `OpenGLMesh`: GPU buffers for one `MeshData` |
| `opengl/shaders/opengl_shader.*` | `OpenGLShader`: compile, bind, set uniforms |
| `opengl/shaders/opengl_shader_library.*` | `ShaderId`, `OpenGLShaderLibrary`: every program, embedded and compiled together |
| `opengl/shaders/glsl/` | GLSL source files |
| `opengl/opengl_font.*` | `OpenGLFont`: glyph atlas texture from a `BitmapFont`, one per `FontId` |
| [debug_renderer.hpp](../../src/renderer/debug_renderer.hpp) | Half-edge debug overlay |

## IRenderer

| Method | Description |
|---|---|
| `initialize(window, surface, config)` | Creates the context, then shaders and shared buffers (`createResources`). `window`/`surface` are the native handles from `Window`. |
| `loadFonts(FontLibrary)` | Builds an atlas texture for every font. Called once by `setupRenderer` after `initialize`. |
| `shutdown()` | Destroys the context. |
| `resize(w, h)` / `setVSync(bool)` / `getVSync()` | Viewport size (also rebuilds the MSAA framebuffer) / swap interval, set by the `vsync` command. |
| `beginFrame()` / `beginMainPass(ClearState)` | Start a frame, clear, and draw the gradient background (when `clearColor` is set). The gradient covers the clear color, which only shows if the background shader fails to load. |
| `setBackground(BackgroundGradient)` / `getBackground()` | Top and bottom background colors. Default top 0.20/0.22/0.28 (Dracula `#343746`), bottom 0.10/0.10/0.13 (`#191a21`). |
| `setLighting(LightingState)` | Lighting for the following `draw` calls. Call once per frame before drawing objects. |
| `setBackFaceTint(color)` / `getBackFaceTint()` | Color back faces are multiplied by. Default 0.8/0.4/0.4. Set by the `backface tint` command. |
| `draw(DrawCommand)` | Draws one mesh's faces, edges, and vertices. |
| `drawGrid(DrawGridCommand)` | Ground grid and axes. |
| `drawText3D(DrawText3DCommand)` | Text on a quad in world space. |
| `drawDebugLine(start, end, mvp)` | One white line. |
| `drawUI(UIDrawList)` | All 2D UI for the frame (console, later the status line and panel) in one call. See [ui.md](ui.md). |
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

    // Point and spot lights share one array (MAX_LOCAL_LIGHTS = 8)
    Vec3 localPositions[8], localDirections[8], localColors[8];   // colors include intensity
    f32 localRanges[8], localCosInner[8], localCosOuter[8];       // cosines of the cone half-angles
    u32 localCount;
};
```

`LightingState` is backend-neutral and doesn't depend on scene types. `buildLightingState` in [application.cpp](../../src/application/application.cpp) fills it each frame: the ambient light, then the camera headlight in slot 0 when it's on (direction = camera forward, color × strength, from `ctx.viewport.headlight`), then enabled directional lights from `scene.lights` in the remaining slots (3 with the headlight on, 4 with it off), then up to 8 enabled point and spot lights. Extra lights past either limit are skipped. A point light is stored as a spot light whose cone covers everything (`cosInner = -1`, `cosOuter = -2`), so the shader has one loop for both.

`DrawText3DCommand` holds references, so it must be used immediately, not stored.

## OpenGL backend

### Anti-aliasing

Everything between `beginMainPass` and `endMainPass` is drawn into an offscreen multisampled framebuffer (`m_msaaFramebuffer`: RGBA8 color + depth/stencil renderbuffers, 4 samples, capped at `GL_MAX_SAMPLES`). `endMainPass` resolves it into the window with `glBlitFramebuffer`. This is used instead of a multisampled window because the WGL context is created with the basic `ChoosePixelFormat`, which can't request MSAA.

- `createRenderTargets(w, h)` builds it (called from `createResources` and `resize`); `destroyRenderTargets()` frees it.
- If the framebuffer can't be created, an error is printed and drawing goes straight to the window without anti-aliasing.
- The grid already anti-aliases its lines in the shader, so MSAA mainly affects mesh edges, points, and face silhouettes.
- Change `m_msaaSamples` in `opengl_renderer.hpp` for more or fewer samples.

### Object drawing

`OpenGLRenderer::draw` uses the `Lit` shader for faces and `Unlit` for everything else:
1. Highlighted faces with `Lit` in `SELECTED_FACE_COLOR` (purple-tinted, so selected faces keep their shading), then all faces with `Lit` in `FACE_COLOR`, with polygon offset so selected faces and all edges draw on top. Lit uniforms are set once by `setLitUniforms`. `Lit` computes `0.7 gray × (ambient + Σ directional + Σ local)`:
   - ambient: `ambientColor × ambientStrength`
   - directional: `color × max(dot(normal, −direction), 0)`
   - local (point/spot): `color × max(dot(normal, toLight), 0) × falloff × cone`, where `falloff = (1 − (distance / range)²)²` (1 at the light, 0 at `range`) and `cone = smoothstep(cosOuter, cosInner, dot(−toLight, direction))`.

   Point and spot lighting varies across a face because it depends on each pixel's world position (`v_WorldPosition` from `lit.vert`). There's no tone mapping, so the sum is clamped at white; strong lights on top of the default ambient, sun, and headlight saturate quickly. Normals come from the face buffer, transformed by `transpose(inverse(model))`. Back faces (seen through holes in open meshes, or a face whose normal got flipped) are lit with the normal flipped, then their base color is multiplied by a pink tint (`u_BackFaceTint`, from `setBackFaceTint`, default 0.95/0.45/0.70, after Dracula pink) so they're recognizable at a glance. On a closed mesh, a pink face means its winding is wrong.
2. Highlighted edges: two translucent purple glow bands (9 px and 5 px, no depth writes) under a crisp 2.5 px purple line. Then all edges in dark gray, 2 px. Selected faces' boundary edges are added to the highlighted edges by `renderFrame`, so selected faces get the same outline.
3. Highlighted vertices, layered like a light marker: a 22 px soft purple glow, an 11 px dark disc, then an 8 px purple disc (only the last writes depth). Then all vertices as 7 px near-black discs. Points use a small depth bias (`u_DepthBias`) so they draw over the edges meeting at them.

Blending is on for edges and vertices; selection colors match the light markers (`light_markers.cpp`).

Colors and sizes are constants at the top of `opengl_renderer_common.cpp`: `FACE_COLOR` (cool gray 0.72/0.73/0.78), `EDGE_COLOR` (0.13/0.13/0.17, Dracula's darker background), `VERTEX_COLOR` (0.10/0.10/0.13), `SELECTED_COLOR` (Dracula purple 0.74/0.58/0.98), `SELECTED_FACE_COLOR` (0.46/0.34/0.74), `SELECTED_GLOW_ALPHA`, and the edge widths and vertex sizes.

Faces are flat-shaded with one normal per triangle, so non-planar faces show their fold. See [features.md](../features.md#lights) for what's next.

### Shaders

Everything shader-related is in `src/renderer/opengl/shaders/`; the platform layer doesn't touch shaders.

GLSL lives in real files under `glsl/` and is compiled into the executable with `#embed` in [opengl_shader_library.cpp](../../src/renderer/opengl/shaders/opengl_shader_library.cpp) (paths are relative to that file). Editing a `.vert`/`.frag` file rebuilds it automatically. Keep GLSL files ASCII.

Programs are looked up by `ShaderId` through `OpenGLShaderLibrary`:

| `ShaderId` | Vertex | Fragment | Used for | Uniforms |
|---|---|---|---|---|
| `Unlit` | `unlit.vert` | `unlit.frag` | Mesh edges, vertices, selection glows, debug lines | `u_MVP`, `u_Color`, `u_Alpha`, `u_DepthBias`, `u_PointShape` (0 square, 1 anti-aliased disc, 2 soft glow; points only, needs `GL_POINT_SPRITE` on in this compatibility context) |
| `Lit` | `lit.vert` | `lit.frag` | Mesh faces | `u_MVP`, `u_Model`, `u_NormalMatrix`, `u_Color`, `u_BackFaceTint`, `u_AmbientColor`, `u_AmbientStrength`, `u_DirectionalLight{Directions,Colors}[4]`, `u_DirectionalLightCount`, `u_LocalLight{Positions,Directions,Colors,Ranges,CosInner,CosOuter}[8]`, `u_LocalLightCount` |
| `WorldText` | `world_text.vert` | `world_text.frag` | Debug labels in world space | `u_MVP`, `u_Color`, `u_Texture` |
| `Background` | `fullscreen.vert` | `background.frag` | Viewport gradient, drawn first with depth test and writes off. Blends `u_BottomColor` → `u_TopColor` by `gl_FragCoord.y / u_ViewportHeight` and adds ±½/255 noise to break up 8-bit banding. | `u_TopColor`, `u_BottomColor`, `u_ViewportHeight` |
| `UI` | `ui.vert` | `ui.frag` | All 2D UI: rounded rects, borders, shadows, lines, ring slices, glyphs (see [ui.md](ui.md#shader)) | `u_ViewportSize`, `u_Texture` |
| `Grid` | `grid.vert` | `grid.frag` | Ground grid and axes | see [Grid](#grid) |

Naming in GLSL: `a_` vertex inputs, `v_` values passed to the fragment stage, `u_` uniforms. `fullscreen.vert` and `grid.vert` take positions from `gl_VertexID` and are drawn with the empty `m_fullscreenVAO`.

`OpenGLShader` methods: `create(name, vert, frag)`, `destroy()`, `bind()`, `setMat4`, `setVec3`, `setVec3Array`, `setFloat`, `setFloatArray`, `setInt` (use for samplers). Array setters take the name of element 0, e.g. `"u_Lights[0]"`. Uniform locations are cached per name. Matrices are uploaded column-major without transposing. Compile and link errors are printed to stderr with the shader's name. A program that fails to load is skipped: `bind()` returns false and the draws that use it return early. The rest of the app keeps running.

**Adding a shader:** write the `.vert`/`.frag` in `glsl/`, add an `#embed` array for each new file and a `ShaderId` entry plus a row in `SOURCES` in `opengl_shader_library.cpp` (a `static_assert` catches a missing row), then `m_shaders.get(ShaderId::...)` in the draw code. `MAX_DIRECTIONAL_LIGHTS` and `MAX_LOCAL_LIGHTS` in `renderer.hpp` and `lit.frag` must match.

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
| `grid.frag` | `LINE_COLOR`, `X_AXIS_COLOR`, `Z_AXIS_COLOR` | Dracula comment blue, red, cyan | Colors |

### GPU meshes

`OpenGLMesh` ([opengl_mesh.hpp](../../src/renderer/opengl/opengl_mesh.hpp)) implements `IMesh` and owns two VAOs:
- **Points and edges:** shared positions (`getVertexData`) and the edge index buffer. Attribute 0 = position.
- **Faces:** the per-triangle corner buffer from `getFaceData` and its index buffer. Attribute 0 = position, 1 = normal.

| Method | Description |
|---|---|
| `create(meshData)` / `update(meshData)` | Rebuild all buffers from `MeshData::getVertexData/getEdgeData/getFaceData`. `update` calls `create` the first time. |
| `drawVertices()` / `drawEdges()` / `drawFaces()` | Draw everything. |
| `drawVertex(h)` / `drawEdge(h)` / `drawFace(h)` | Draw one element, using the index maps from the export to find its range. Used for highlights. |
| `destroy()` | Free GL objects. |

Normals are computed on the CPU during export, so they're only recalculated when the mesh is marked dirty, not every frame.

### Text

`BitmapFont` ([bitmap_font.hpp](../../src/core/font/bitmap_font.hpp)) loads a `.bmf` file: a text format with the glyph size in its header and one hex digit (16 alpha levels) per pixel, for ASCII 32–126. Fonts are compiled into the binary with `#embed` ([embedded_fonts.cpp](../../src/core/font/embedded_fonts.cpp); assets path set by `--embed-dir` in CMake) and loaded by `FontLibrary`. `OpenGLFont` uploads one font into a 16 × 6 atlas texture; `FontAtlas::glyphUV` gives each character's UVs.

Screen-space text goes through the UI draw list ([ui.md](ui.md)). `drawText3D` draws world-space text with the console font.

`tools/scale_bmf.py` makes a smaller `.bmf` from a larger one (used for the UI font); `tools/fix_bmf_baseline.py` adjusts glyph baselines in the 16×24 console font.

## Debug renderer

`DebugRenderer::render(renderer, scene, viewProjection)` runs when the `Debug` context is on (console command `debug`). For every half-edge of every object it draws an arrow, offset toward its face (or away, for border edges) so the two halves of an edge are both visible, plus an `index:generation` label using `drawText3D`.

## Adding a backend

Implement `IRenderer` and `IMesh`, add a case to `createRenderer`. The application's `ObjectMeshCache` holds `OpenGLMesh` per object, so it would need to create the new backend's mesh type instead.
