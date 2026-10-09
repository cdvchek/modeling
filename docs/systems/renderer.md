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
| `opengl/opengl_renderer_common.cpp` | Portable GL code: `createResources`/`destroyResources`, textures, and drawing objects, images, grid, world text, debug lines |
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
| `setExposure(stops)` / `getExposure()` | Multiplies lit surfaces by 2^stops before tone mapping. Set every frame from `ctx.viewport.exposure`. |
| `setBackFaceTint(color)` / `getBackFaceTint()` | Color back faces are multiplied by. Default 0.95/0.45/0.70 (pink). Set by the `backface tint` command. |
| `draw(DrawCommand)` | Draws one mesh's faces, edges, and vertices. |
| `drawGrid(DrawGridCommand)` | Ground grid and axes. |
| `drawImage(DrawImageCommand)` | A textured unit square (reference images): blended, with an opacity on top of the texture's alpha. Writes depth only when depth tested and fully opaque, so see-through images don't hide what's drawn after them. |
| `renderMaterialPreview(look, size, texture)` | A material swatch ([opengl_material_preview.cpp](../../src/renderer/opengl/opengl_material_preview.cpp)): a smooth 64 × 32 sphere in the `SurfaceLook`, drawn with the `Lit` shader under fixed studio lighting (a warm key light, a cool fill, a gray sky and ground, no exposure) so every swatch is lit the same whatever the scene, over an 8 × 8 checkerboard when it's see-through, into a multisampled target that's resolved into `texture` (made when 0) with mipmaps. Call outside the main pass; it restores the viewport. |
| `createTexture(pixels, width, height)` / `destroyTexture(texture)` | An RGBA8 texture (rows from the top) with mipmaps, trilinear filtering, and clamped edges; returns 0 if it's larger than `GL_MAX_TEXTURE_SIZE`. |
| `drawText3D(DrawText3DCommand)` | Text on a quad in world space. |
| `drawDebugLine(start, end, mvp)` | One white line. |
| `getStats()` | The last finished frame's `RenderStats` ([render_stats.hpp](../../src/renderer/render_stats.hpp)): draw calls, triangles, lines, points, uniform uploads, mesh rebuilds and patches with their bytes, and GPU time. Every draw call in the backend goes through `drawElements` / `drawArrays` in [opengl_counters.hpp](../../src/renderer/opengl/opengl_counters.hpp), which count it, and every `OpenGLShader` set counts as an upload; `beginFrame` resets the counts and `endFrame` keeps them. GPU time comes from `GL_TIME_ELAPSED` queries in a ring of four, read back once ready so reading never stalls (a few frames late; a frame whose slot isn't ready isn't timed). |
| `drawUI(UIDrawList)` | All 2D UI for the frame (markers, status bar, panel, windows, menus, console) in one call. See [ui.md](ui.md). |
| `endMainPass()` / `endFrame()` / `present()` | Resolve the MSAA framebuffer to the window, finish, and swap buffers. |

### Draw commands

```cpp
struct DrawCommand {
    bool showVerts, showEdges, showFaces;
    std::vector<VertexHandle> highlightedVerts;   // drawn in the selection purple
    std::vector<EdgeHandle> highlightedEdges;
    std::vector<FaceHandle> highlightedFaces;
    IMesh* mesh;
    Mat4 model;           // for the normal matrix
    Mat4 mvp;
    SurfaceLook surface;  // the object's look: every face when parts is empty, and selected faces' tint
    std::vector<DrawPart> parts;   // faces by material: firstIndex, indexCount, surface (one FaceGroup each)
    bool outlineAll;      // object mode: every edge in the selection style, one draw per pass
    std::vector<EdgeHandle> hardEdges;   // edit mode: edges that shade hard (MeshData::getHardEdges)
};

struct SurfaceLook {      // a material's values, colors sRGB
    Vec3 baseColor;       // default: the clay gray 0.72/0.73/0.78
    f32 roughness = 0.5, metallic = 0;
    Vec3 emissiveColor; f32 emissiveStrength = 0;
    bool blend = false;   // see-through by opacity; no depth writes; draw after everything solid
    f32 opacity = 1;
    BackFaces backFaces = BackFaces::Tinted;   // Tinted (clay view), Lit (double-sided), Culled
};

struct DrawImageCommand {
    u32 texture;           // from createTexture
    Mat4 mvp;              // places the unit square (-0.5 to 0.5 in X and Y)
    f32 opacity;
    bool depthTest;        // false draws it over whatever is there
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
1. Faces with `Lit`: highlighted faces first, in one draw (`drawFaceSet`), with the object's look and its base color mixed `SELECTED_FACE_MIX` (0.8) of the way to `SELECTED_FACE_COLOR` (so selected faces keep their shading); then each part (one run of a material's faces, `drawFaceRange`), or every face in `surface` when there are no parts, with polygon offset so selected faces and all edges draw on top. Per object, `setLitUniforms` sends only the three matrices; the lights come from the **Lighting uniform buffer** (`uploadLighting`: lights, sky and ground, camera position, exposure, back-face tint, in one std140 block at binding 0, sent again only when `setLighting`, `setExposure`, or `setBackFaceTint` changed something), and each part's material through `setSurfaceUniforms`, which skips sending a material equal to the last one (`sameSurface`).
   - **Back faces:** `BackFaces::Tinted` draws them lit with the normal flipped and their base color multiplied by a pink tint (`u_BackFaceTint`, from `setBackFaceTint`, default 0.95/0.45/0.70, after Dracula pink), so flipped or open faces stand out (on a closed mesh, a pink face means its winding is wrong); `Lit` draws them like front faces (double-sided materials); `Culled` turns on `GL_CULL_FACE` so they aren't drawn. Counterclockwise is the front.
   - **Blend:** blending on, depth writes off, `u_Opacity` from the look. A blended mesh that shows its back faces is drawn twice, back faces first (front ones culled), then front faces, so the far side of a glass shows behind the near side. The caller draws blended meshes after everything solid, farthest first (see [application.md](application.md#main-loop)).

   **Shading** (`lit.frag`) is the metallic-roughness model glTF and the engines use, in linear color:
   - The base color, emissive color, highlight color, and back-face tint arrive as sRGB and are converted. Light colors in `LightingState` are already linear (`buildLightingState` converts the picked sRGB colors with `srgbToLinear` from [color.hpp](../../src/core/math/color.hpp)) and include their intensity.
   - Each light adds Lambert diffuse plus a GGX highlight (Smith height-correlated visibility, Schlick Fresnel; reflectance 0.04 for non-metals, the base color for metals, whose diffuse goes to zero), scaled by π so a plain diffuse surface lights as `color × cos`. Directional: `max(dot(normal, −direction), 0)`. Point and spot: also `falloff × cone`, where `falloff = (1 − (distance / range)²)²` (1 at the light, 0 at `range`) and `cone = smoothstep(cosOuter, cosInner, dot(−toLight, direction))`.
   - **Environment:** a sky color above and a ground color below (`LightingState::skyColor`, `groundColor`): the ambient light (color × strength) shaped by the background gradient's hue (half of it, so gray surfaces don't turn blue), ×1.4 for the sky and ×0.6 for the ground, so they average to the ambient light. Diffuse ambient looks up the environment along the normal, fully blurred; reflections look it up along the reflected view direction, with the horizon softened more as roughness rises, weighted by an analytic stand-in for a prefiltered environment's reflectance (Karis's mobile approximation). This is why metals aren't black without an environment image.
   - Emissive color × strength is added, then everything is multiplied by `2^exposure`, tone mapped with Khronos PBR Neutral (unchanged below about 0.76, then rolled off toward white), and converted back to sRGB.

   **UV checker** (`LightingState::uvChecker`, set from `ctx.viewport.showUVChecker`): the base color is replaced by a colored grid read from the face's UVs (`v_UV`, attribute 2): 8 × 8 cells, hue by column and darker by row so each cell is unique, alternate cells lighter, thin lines between cells (`fwidth`, so they stay one pixel wide). Stretched or flipped cells show bad UVs at a glance. It's sent in `u_CameraPosition.w`.

   Point and spot lighting varies across a face because it depends on each pixel's world position (`v_WorldPosition` from `lit.vert`). Normals come from the face buffer, transformed by `transpose(inverse(model))`.
2. Highlighted edges (one draw per pass with `drawEdgeSet`, or the whole wireframe with `outlineAll`): two translucent purple glow bands (9 px and 5 px, no depth writes) under a crisp 2.5 px purple line. Then hard edges (`hardEdges`, smooth-shaded meshes in edit mode) in Dracula cyan, 2.5 px, with `drawHardEdgeSet`, under the selection (the depth test keeps the line drawn first). Then all edges in dark gray, 2 px. Selected faces' boundary edges are added to the highlighted edges by `renderFrame`, so selected faces get the same outline.
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
| `Lit` | `lit.vert` | `lit.frag` | Mesh faces | Per object: `u_MVP`, `u_Model`, `u_NormalMatrix`. Per material: `u_BaseColor`, `u_Roughness`, `u_Metallic`, `u_EmissiveColor`, `u_EmissiveStrength`, `u_Opacity`, `u_BackFaces` (0 tinted, 1 lit); `u_Highlight`, `u_HighlightColor` (colors sRGB). The **Lighting** uniform block (std140, binding 0): `u_DirectionalLight{Directions,Colors}[4]`, `u_LocalLight{Positions,Directions,Colors}[8]` (range, inner and outer cone cosines in their w), `u_SkyColor` (exposure multiplier in w), `u_GroundColor`, `u_CameraPosition` (UV checker on in w), `u_BackFaceTint`, `u_LightCounts` |
| `WorldText` | `world_text.vert` | `world_text.frag` | Debug labels in world space | `u_MVP`, `u_Color`, `u_Texture` |
| `Background` | `fullscreen.vert` | `background.frag` | Viewport gradient, drawn first with depth test and writes off. Blends `u_BottomColor` → `u_TopColor` by `gl_FragCoord.y / u_ViewportHeight` and adds ±½/255 noise to break up 8-bit banding. | `u_TopColor`, `u_BottomColor`, `u_ViewportHeight` |
| `UI` | `ui.vert` | `ui.frag` | All 2D UI: rounded rects, borders, shadows, lines, ring slices, gradients, glyphs, images (premultiplied textures such as material swatches) (see [ui.md](ui.md#shader)) | `u_ViewportSize`, `u_Texture` |
| `Grid` | `grid.vert` | `grid.frag` | Ground grid and axes | see [Grid](#grid) |
| `Image` | `image.vert` | `image.frag` | Reference images: a unit square from `gl_VertexID` (6 vertices, the empty VAO), the texture's first row at the top; fragments under 0.4% alpha are discarded so they leave the depth buffer alone | `u_MVP`, `u_Opacity`, `u_Texture` |

Naming in GLSL: `a_` vertex inputs, `v_` values passed to the fragment stage, `u_` uniforms. `fullscreen.vert`, `grid.vert`, and `image.vert` take positions from `gl_VertexID` and are drawn with the empty `m_fullscreenVAO`.

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
- **Axes:** the X axis (`z = 0`) is drawn red and the Z axis (`x = 0`) cyan, 2 px wide.
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

`OpenGLMesh` ([opengl_mesh.hpp](../../src/renderer/opengl/opengl_mesh.hpp)) implements `IMesh` and owns two VAOs, plus one small VAO and index buffer per selection set:
- **Points and edges:** shared positions (`getVertexData`) and the edge index buffer. Attribute 0 = position.
- **Faces:** the per-triangle corner buffer from `getFaceData` and its index buffer. Attribute 0 = position, 1 = normal, 2 = UV (the face-set VAOs use the same layout).

| Method | Description |
|---|---|
| `create(meshData, groupOf, groupingStamp)` / `update(...)` | Rebuild all buffers from `MeshData::getVertexData/getEdgeData/getFaceData(groupOf)`, the faces grouped by material. `update` calls `create` the first time. Keeps the mesh's `MeshStamp`, the grouping stamp (the material collection's), and CPU copies of the position and face buffers. |
| `patch(meshData, groupingStamp)` | After vertices only moved: if both stamps still match, writes the moved vertices' positions and every face `getFacesToPatch` names (the faces around them, more with smooth shading; their corners again, through `appendFaceCorners`, into the same place, since a face keeps its triangle count) into the CPU copies, then sends one `glBufferSubData` per buffer covering the changed range. Returns false (and the caller rebuilds) when the layout or the grouping changed, or a face's corner count did. Edge and selection index buffers don't change. |
| `drawVertices()` / `drawEdges()` / `drawFaces()` | Draw everything. |
| `faceGroups()` / `drawFaceRange(first, count)` | The faces' runs, one per material, and one run's draw. |
| `drawHardEdgeSet(handles)` | Like `drawEdgeSet`, in a set of its own so the selection and the hard edges don't upload over each other every frame. |
| `drawVertexSet(handles)` / `drawEdgeSet(handles)` / `drawFaceSet(handles)` | A selection in one draw call: the set's indices (from the export's index maps) go into its own index buffer, uploaded again only when the set's handles (or the mesh) change, so drawing the same selection every frame costs one call. |
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
