# UI

Everything drawn on top of the viewport (light markers, status bar, console) and the immediate-mode widget system that panels are built from (see the [roadmap](../features.md#planned) for the floating panel and tabs).

Files: `src/ui/` (part of `modeling_core`, no OpenGL), drawn by [opengl_ui_renderer.cpp](../../src/renderer/opengl/opengl_ui_renderer.cpp).

| File | Contents |
|---|---|
| [ui_types.hpp](../../src/ui/ui_types.hpp) | `Rect`, `Color`, `UIFont`, `makeUIFont`, `measureText` |
| [ui_draw_list.hpp](../../src/ui/ui_draw_list.hpp) | `UIDrawList`, `UIVertex`, `UIDrawBatch` |
| [ui_input.hpp](../../src/ui/ui_input.hpp) | `UIInput`: one frame of mouse state, filled by the app |
| [ui_context.hpp](../../src/ui/ui_context.hpp), `ui_context.cpp` | `UIContext`: input routing, IDs, hot/active, regions, layout |
| `ui_widgets.cpp` | The widgets |
| [ui_style.hpp](../../src/ui/ui_style.hpp) | `UIStyle`: every widget size and color |

## How a frame draws UI

1. `renderFrame` clears `ctx.uiDrawList`.
2. UI code adds shapes and text to it, in drawing order: `drawLightMarkers`, `drawStatusBar`, the widget pass (`ctx.ui.beginDraw()` … `endDraw()`, currently the floating panel), then `drawConsole` (so the open console dims everything under it).
3. `ctx.renderer->drawUI(list)` uploads the whole list once and draws it after the scene, grid, and debug overlay, before the MSAA resolve.

UI code never calls OpenGL, so everything up to step 3 can be unit tested (see `tests/ui_draw_list_tests.cpp`).

## Widgets (UIContext)

`ctx.ui` is an immediate-mode UI: there are no widget objects. Each frame, code calls functions like `ui.sliderFloat("Intensity", light.intensity, 0, 5)`, and each call draws the widget and handles its input right there, returning true when it changed the value. Because the UI reads the scene every frame, it's never out of sync with undo, console edits, or tools.

### Frame flow and input routing

```
main loop:
    input.beginFrame(); pollEvents()
    ui.beginFrame(makeUIInput(input), interactive)   // decides wantsMouse from last frame's regions
    actions.setMouseBlocked(ui.wantsMouse())          // viewport mouse actions go quiet
    checkActions(ctx)                                 // tools, picking, camera
    renderFrame(ctx)                                  // ... ui.beginDraw(); regions + widgets; ui.endDraw() ...
```

- **Regions** (`beginRegion(rect)` / `endRegion()`) are the areas that block the viewport, like a panel. They also clip their contents and start a top-to-bottom layout. Hover is tested against the regions drawn the **previous** frame, because `wantsMouse` has to be known before `checkActions` runs.
- **`wantsMouse()`** is true when the mouse is over a region or a widget is being dragged. `ActionMap::setMouseBlocked` then makes every action bound to a mouse button or the scroll wheel return false (select, loop/ring select, orbit, pan, zoom), while keyboard shortcuts keep working.
- **Drags belong to whoever started them.** A press outside the UI (e.g. starting an orbit) belongs to the viewport until every button is released, so crossing a panel doesn't steal it. A widget being dragged keeps the mouse even outside its region.
- **Not interactive** while the console is open or a modal tool (grab, scale, rotate, bevel) runs: widgets still draw but ignore the mouse, and `wantsMouse` is false so the tool's confirm/cancel clicks always reach it.
- `beginDraw`/`endDraw` can run more than once per frame (Windows redraws during a resize); `endDraw` clears the frame's clicks so they're only handled once.

### IDs, hot, and active

- Each widget's ID is a hash of its label combined with the ID stack (`pushId(name)` / `pushId(number)` / `popId()`). Widgets with the same label need different scopes, e.g. `pushId(handle.index)` around each light's controls. Every region pushes its own index.
- **Hot** = the widget under the mouse (hover look). **Active** = the widget the left button was pressed on, held until release. Only one widget is active; a button clicks on release over itself; a slider keeps tracking after the mouse leaves it.
- If the active widget stops being drawn (e.g. its light was deleted mid-drag), it's released at the end of the frame.

### Layout

Inside a region, each widget takes the next row (`UIStyle::ROW_HEIGHT`, 24 px) across the full width, minus `UIStyle::PADDING`. Labeled widgets split the row: the label in a dim left column (`LABEL_FRACTION` of the width), the control on the right. `spacing()`, `separator()`, and `indent(px)` adjust the flow.

### Widget list

| Widget | Look and behavior | Returns true when |
|---|---|---|
| `label(text, dim)` | A line of text | — |
| `heading(text)` | Amber section title | — |
| `button(label)` | Full-width rounded button, centered text | Clicked (released over itself) |
| `selectable(label, selected)` | Full-width row; selected rows get an amber tint and a 2 px amber bar on the left | Clicked |
| `checkbox(label, value)` | Amber box with a check mark when on; toggles on release | Toggled |
| `sliderFloat(label, value, min, max, format)` | Box filled in amber up to the value, number centered; the value follows the mouse's x while held | Value changed |
| `dragFloat3(label, vec3, speed, format)` | Three boxes with X/Y/Z in axis colors; dragging left/right nudges that component by `mouseDelta × speed` | Any component changed |
| `colorEdit(label, rgb)` | A swatch of the color; clicking it shows or hides R/G/B sliders below it | A channel changed |

After any widget, `isItemHovered()`, `isItemActivated()`, `isItemDeactivated()`, and `isItemDeactivatedAfterEdit()` describe it (for grouped widgets like `dragFloat3` and `colorEdit`, the group as a whole).

### Undo

Widgets never change a value on the frame they activate. That's what makes undo grouping work: call `trackUndo(ctx)` ([ui_undo.hpp](../../src/application/ui_undo.hpp)) right after a widget, and it calls `history.begin` on activation (before any change) and `history.commit` on release, so a whole drag is one undo step. A press that didn't change anything is cancelled instead. When editing scene data through widgets, edit a copy and write it back after (see `main_panel.cpp`): `history.cancel` replaces the scene's containers, so a reference held across it could dangle.

### Style

All sizes and colors live in `UIStyle`. The accent is the same warm amber as viewport selection; X/Y/Z use the grid's axis colors.

### Floating panel

`ui.beginPanel(name, state, bounds, title)` … `ui.endPanel()` wraps widgets in a floating panel:

- `UIPanelState` (just a `Rect`) is owned by the app and kept between frames; the main panel's lives in `ctx.viewport.panel`.
- Draws a soft drop shadow, a rounded background with a 1 px border, and a 32 px header strip (`PANEL_HEADER_HEIGHT`) with the title. The header lightens on hover and while dragging.
- **Dragging:** press on the header and move. The offset where you grabbed is remembered (`m_panelGrabOffset`), so the panel doesn't jump.
- **Resizing:** drag any edge or corner. The grab zone is `PANEL_RESIZE_GRIP` (5 px) on each side of the border, so edges can be grabbed from just outside the panel; the region is widened by that much to make it hoverable. The top edge wins over the header drag. The opposite edges stay put, the size never goes below `PANEL_MIN_WIDTH` × `PANEL_MIN_HEIGHT` (240 × 160), and dragged edges stop at the bounds. The border turns amber while an edge is hovered or being dragged.
- **Cursor:** `ui.cursor()` reports the cursor the UI wants (`UICursor`: arrow, horizontal, vertical, or one of two diagonals) based on the hovered or dragged edges; it stays a resize cursor for the whole drag. The main loop passes it to `Window::setCursor` after each frame.
- **Kept inside `bounds`** on every frame, not just while dragging: a smaller window first shrinks the panel to fit, then moves it inside.
- The whole panel is a region (blocks the viewport); widgets are laid out below the header.

[main_panel.cpp](../../src/application/main_panel.cpp) is the app's panel: it's placed at the top right on first use, bounded by the viewport above the status bar, and toggled with `ui panel`. Its contents are currently the light controls from the UI core work (ambient, headlight, light list, add light, selected light's properties); tabs come next.

## Coordinates and types

Pixels, origin at the **top-left**, y pointing **down**.

| Type | Description |
|---|---|
| `Rect { x, y, width, height }` | `right()`, `bottom()`, `center()`, `contains(point)`, `Rect::intersect(a, b)` |
| `Color { r, g, b, a }` | 0..1 floats. `packed()` turns it into RGBA bytes for the vertex. |
| `UIFont { id, glyphWidth, glyphHeight }` | Which font texture to use and its cell size. Get one with `makeUIFont(FontId, ctx.fonts.get(FontId))`. |
| `measureText(font, text)` | Width and height of monospaced text; `\n` starts a new line. Use it for layout (centering, sizing tabs). |

## UIDrawList

| Method | Description |
|---|---|
| `rect(rect, fill)` | Filled rectangle. |
| `roundedRect(rect, radius, fill, border = {}, borderWidth = 0)` | Rounded rectangle with an optional inside border. Radius is clamped to half the shorter side. |
| `shadow(rect, radius, blur, color)` | Soft shadow shaped like a rounded rect, fading over `blur` pixels on each side of its edge. Draw it before the panel it belongs to, usually offset a few pixels down. |
| `line(start, end, width, color)` | Anti-aliased line of any width (drawn as a thin rotated rectangle, so no `glLineWidth` limits). |
| `text(position, text, font, color)` | One quad per character from the top-left `position`, snapped to whole pixels so glyphs stay crisp. Spaces advance without drawing; `\n` starts a new line. |
| `pushClip(rect)` / `popClip()` | Clip everything added until the matching pop. Nested clips intersect. |
| `clear()` | Empties the list; called once per frame. |

Items draw in the order they're added: later items cover earlier ones.

### Batches

Every item becomes 4 vertices and 6 indices. Consecutive items share a `UIDrawBatch` (one draw call) until the clip rect changes or text needs a different font texture than the batch already uses. Shapes don't need a texture, so shapes and text in one font mix freely: a whole panel is usually 1–3 batches.

### UIVertex

```cpp
struct UIVertex {
    Vec2 position;      // pixels
    Vec2 uv;            // font atlas, glyphs only
    Vec2 local;         // offset from the shape's center in the shape's own axes
    Vec2 halfSize;      // shape half extents
    f32 radius, borderWidth, blur, mode;   // mode: MODE_SHAPE or MODE_GLYPH
    u32 fill, border;   // packed RGBA
};
```

Shape quads are padded by `1 + blur` pixels past the shape so the anti-aliased edge and shadow have room to fade.

## Shader

`ShaderId::UI` ([ui.vert](../../src/renderer/opengl/shaders/glsl/ui.vert), [ui.frag](../../src/renderer/opengl/shaders/glsl/ui.frag)). The vertex shader converts pixels to clip space with `u_ViewportSize`. The fragment shader:

- **Glyphs:** `fill` with alpha × the atlas texel.
- **Shapes:** signed distance from `local` to a rounded rectangle of `halfSize` and `radius`.
  - Coverage `clamp(0.5 − distance, 0, 1)` gives a 1 px anti-aliased edge.
  - With a border, the color blends from `border` to `fill` one border width inside the edge.
  - With `blur > 0`, alpha is `1 − smoothstep(−blur, blur, distance)`: a soft shadow.

`OpenGLUIRenderer::draw` turns off depth testing and writes, turns on alpha blending, sets `glScissor` per clipped batch (converting y-down clip rects to GL's y-up), binds the batch's font texture, and restores 3D state afterward.

## Fonts

[font_library.hpp](../../src/core/font/font_library.hpp) loads every embedded font; `ctx.fonts` holds them and `setupRenderer` passes them to `renderer->loadFonts`, which builds one atlas texture per font.

| `FontId` | File | Cell | Used for |
|---|---|---|---|
| `Console` | `assets/fonts/console.bmf` | 16×24 | Console, world-space debug labels |
| `UI` | `assets/fonts/ui.bmf` | 10×16 | Panels, status line |

Glyphs are stored in a 16 × 6 atlas grid in character order, so UVs come from `FontAtlas::glyphUV(char)` ([font_atlas.hpp](../../src/core/font/font_atlas.hpp)) without knowing the glyph size. Characters outside ASCII 32–126 draw as spaces.

`ui.bmf` is generated from `console.bmf`: each 16×24 cell is trimmed to its 14×22 inked area plus a 1 px margin, then area-averaged down to 10×16, so it's the same typeface at a smaller size. Regenerate it after changing the console font with `python tools/scale_bmf.py assets/fonts/console.bmf assets/fonts/ui.bmf 10 16 1 1 15 23`. To add a font: generate a `.bmf`, embed it in `embedded_fonts.cpp`, add a `FontId`, and load it in `FontLibrary::loadEmbedded`.
