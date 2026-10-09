# UI

Everything drawn on top of the viewport (markers, tool guides, status bar, panel, modal windows, radial menu, console) and the immediate-mode widget system the panel and modal windows are built from.

Files: `src/ui/` (part of `modeling_core`, no OpenGL), drawn by [opengl_ui_renderer.cpp](../../src/renderer/opengl/opengl_ui_renderer.cpp).

| File | Contents |
|---|---|
| [ui_types.hpp](../../src/ui/ui_types.hpp) | `Rect`, `Color`, `UIFont`, `makeUIFont`, `measureText`, `fitText` |
| [ui_draw_list.hpp](../../src/ui/ui_draw_list.hpp) | `UIDrawList`, `UIVertex`, `UIDrawBatch` |
| [ui_input.hpp](../../src/ui/ui_input.hpp) | `UIInput`: one frame of mouse and keyboard state, filled by the app: mouse buttons, wheel, typed `text`, editing `keys` (`UIKey`: arrows, Home/End, Backspace/Delete, Enter, Escape, and Ctrl+A/C/X/V; repeats included), `shift`, and `time` for the caret blink |
| [ui_context.hpp](../../src/ui/ui_context.hpp), `ui_context.cpp` | `UIContext`: input routing, IDs, hot/active, regions, layout |
| `ui_widgets.cpp` | The widgets |
| [ui_style.hpp](../../src/ui/ui_style.hpp) | `UIStyle`: every widget size and color |

## How a frame draws UI

1. `renderFrame` clears `ctx.uiDrawList`.
2. UI code adds shapes and text to it, in drawing order: `drawLightMarkers`, `drawOriginMarkers`, `drawToolGuides`, `drawStatusBar`, the widget pass (`ctx.ui.beginDraw()` … `endDraw()`: the floating panel, then an open modal window, then any open dropdown list), then `drawRadialMenu`, then `drawConsole` (so the open console is on top).
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
    actions.setKeyboardBlocked(ui.wantsKeyboard())    // shortcuts go quiet while a text field is edited
    checkActions(ctx)                                 // tools, picking, camera
    renderFrame(ctx)                                  // ... ui.beginDraw(); regions + widgets; ui.endDraw() ...
```

- **Regions** (`beginRegion(rect)` / `endRegion()`) are the areas that block the viewport, like a panel. They also clip their contents and start a top-to-bottom layout. Hover is tested against the regions drawn the **previous** frame, because `wantsMouse` has to be known before `checkActions` runs.
- **`wantsMouse()`** is true when the mouse is over a region or a widget is being dragged. `ActionMap::setMouseBlocked` then makes every action bound to a mouse button or the scroll wheel return false (select, loop/ring select, orbit, pan, zoom), while keyboard shortcuts keep working.
- **Drags belong to whoever started them.** A press outside the UI (e.g. starting an orbit) belongs to the viewport until every button is released, so crossing a panel doesn't steal it. A widget being dragged keeps the mouse even outside its region.
- **Not interactive** while the console is open or a modal tool (grab, scale, rotate, bevel) runs: widgets still draw but ignore the mouse, and `wantsMouse` is false so the tool's confirm/cancel clicks always reach it.
- `beginDraw`/`endDraw` can run more than once per frame (Windows redraws during a resize); `endDraw` clears the frame's clicks so they're only handled once.

### IDs, hot, and active

- Each widget's ID is a hash of its label combined with the ID stack (`pushId(name)` / `pushId(number)` / `popId()`). Widgets with the same label need different scopes, e.g. `pushId(handle.index)` around each light's controls. Every region pushes its own index, and the main panel scopes each section (`ambient`, `headlight`, `lights`, `selected light`). If two widgets end up with the same ID in one frame they'd act as one (dragging either moves both), so `interact` prints a warning naming the label the first time it happens.
- **Hot** = the widget under the mouse (hover look). **Active** = the widget the left button was pressed on, held until release. Only one widget is active; a button clicks on release over itself; a slider keeps tracking after the mouse leaves it.
- If the active widget stops being drawn (e.g. its light was deleted mid-drag), it's released at the end of the frame.

### Layout

Inside a region, each widget takes the next row (`UIStyle::ROW_HEIGHT`, 24 px) across the full width, minus `UIStyle::PADDING`. Labeled widgets split the row: the label in a dim left column (`LABEL_FRACTION`, 32% of the width), the control on the right. A label that doesn't fit its column is shortened with "..." (`fitText`), as are `label` text and list rows, so nothing runs into its neighbor when the panel is narrow. `spacing()`, `separator()`, and `indent(px)` adjust the flow. `row(height)` hands out the next row's rect and `text(rect, text, color)` draws a label in it, for custom rows like a heading with buttons on the right.

**Child boxes:** `beginChild(name, height)` … `endChild()` reserves a fixed-height box (darker background, 1 px border) whose content scrolls on its own, with its own scrollbar. Its scroll state is kept per ID. The wheel over a child scrolls the child and is used up, so the panel doesn't also scroll; that's why the panel handles the wheel in `endPanel`, after its children. Widgets in the child only respond inside the box (intersected with the panel's content area).

### Widget list

| Widget | Look and behavior | Returns true when |
|---|---|---|
| `label(text, dim)` | A line of text, shortened with "..." if it doesn't fit | — |
| `heading(text)` | Green section title | — |
| `button(label)` | Full-width rounded button, centered text | Clicked (released over itself) |
| `treeRow(label, selected, detail, depth, hasChildren, open)` | A `selectable` indented by `depth`, with a small arrow that folds the row's children away (`open` toggles on a click on the arrow, which doesn't select the row). The label is shortened to fit | The row (not the arrow) was clicked |
| `selectable(label, selected, detail, icon)` | Full-width row; selected rows get a purple tint and a 2 px purple bar on the left. Optional `detail` is drawn dim and right-aligned; the label gets the rest of the row and is shortened with "..." rather than running into it. Optional `icon`, a renderer texture (a material swatch), is drawn as a small square before the label | Clicked |
| `button(label, rect, enabled)` | A button at an explicit rect, for putting several controls on one row. Disabled buttons are drawn dim and ignore the mouse | Clicked |
| `segmented(id, rect, index, options)` | The same switch in a given rect with no label (table rows); an `index` of -1 highlights nothing | A different option was picked |
| `segmented(label, index, options)` | A row of options in one frame; the picked one is filled purple. Segments are sized to their text and share the leftover width; each label is clipped to its segment. An empty label uses the full row | A different option was picked (on release) |
| `dropdown(label, rect, index, options, icons)` | A button showing `options[index]` with a chevron; clicking it opens a list below it (or above, if it wouldn't fit in the viewport) drawn on top of everything. Picking an option closes the list | An option was picked (the frame after the click) |
| `checkbox(label, value)` | Purple box with a check mark when on; toggles on release | Toggled |
| `checkbox(id, rect, value)` | Just the box, at the left of `rect` and centered in its height; the whole rect is clickable (table rows) | Toggled |
| `sliderFloat(label, value, min, max, format)` | Box filled in purple up to the value, number centered; the value follows the mouse's x while held | Value changed |
| `dragFloat3(label, vec3, speed, format)` | Three boxes with X/Y/Z in axis colors and the value right-aligned beside the letter; a value too wide for its box shows fewer decimals (`-3.25` → `-3.2` → `-3`) and is clipped rather than drawn over the letter. Dragging left/right nudges that component by `mouseDelta × speed` once the mouse has moved 3 px (`DRAG_THRESHOLD`). A press and release under that opens the component as a text edit (value as `%g`, all selected); Enter or a click elsewhere parses it (a non-number is ignored) | Any component changed (by drag, or when a typed value is kept) |
| `dragFloat(label, value, speed, format)` | One box with no axis letter, dragged and typed the same way as `dragFloat3` (both are drawn by `dragFloats`) | The value changed |
| `textField(label, text, allowEmpty = false)` | A field showing `text`; pressing it starts an edit with all text selected (I-beam cursor over it) | Once, when an edit is kept that changed the text (trimmed; an empty edit is dropped unless `allowEmpty`) |
| `colorEdit(label, rgb)` | A swatch of the color; clicking it opens or closes a picker below it: a saturation (left to right) / value (top to bottom) square with a hue strip beside it, drawn with `gradientRect`, a **Hex** text field (`#rrggbb`, also `rrggbb` and `#rgb`), and typed **RGB** boxes (0 to 1, dragged or typed like `dragFloat3`). While open, the swatch shows the color it opened with on the left and the current one on the right, once they differ. The picker keeps its own hue while the color is gray or black, so dragging into the corner and back doesn't lose it. Every part folds into one item, so a drag or a typed value is one undo step | The color changed |
| `dragFloat(label, value, speed, format)` | One box with no axis letter, dragged and typed like `dragFloat3` | The value changed |

After any widget, `isItemHovered()`, `isItemActivated()`, `isItemDeactivated()`, and `isItemDeactivatedAfterEdit()` describe it (for grouped widgets like `dragFloat3` and `colorEdit`, the group as a whole).

### Text editing

One field at a time is edited; its state (`TextEdit`: buffer, caret, selection anchor, horizontal scroll) lives in the context, keyed by the widget's ID, and the widget draws it with `textEditBox` instead of its normal look.
- **Mouse:** a press inside places the caret (Shift+click extends the selection) and dragging selects; a press anywhere outside **commits**. That press doesn't reach the viewport (`wantsMouse` is true for it), but other UI widgets still get it, so clicking another list row commits and then selects the row.
- **Keys:** handled in the order pressed. Left/Right/Home/End move the caret (Shift extends; without Shift, Left/Right first collapse a selection), Backspace/Delete remove a character or the selection, Ctrl+A selects all, Ctrl+C/X/V use the clipboard functions given to `setClipboard(get, set)` (pasted text is cut at the first line break and limited to printable ASCII), Enter commits, Escape cancels. Typed `text` replaces the selection.
- **Drawing:** the field gets the active frame color and a green border; the selection is purple behind the text, the caret a 1 px green line that blinks every 0.5 s and stays solid briefly after each edit. Text scrolls sideways to keep the caret in view.
- **Undo:** the whole edit is one interaction. The commit frame reports `isItemActivated()` and `isItemDeactivated()` together (and `isItemDeactivatedAfterEdit()` if the value changed), so `trackUndo` makes it a single step.
- **Keyboard ownership:** `wantsKeyboard()` is true while a field is edited. The app passes it to `ActionMap::setKeyboardBlocked`, so no key shortcut fires (Quit excepted).
- **Ending without a commit:** an edit whose widget isn't drawn in a frame (tab switched, object removed) or that becomes non-interactive is dropped.

### Drag and drop

Right after a widget, `dragSource(payload, label)` starts a drag carrying `payload` (a number, e.g. an object's slot) once that widget has been pressed and the mouse has moved past the 3 px drag threshold; it returns true while its drag lasts. A small tag with `label` follows the mouse (drawn in `endDraw`, over everything). On the release, the first `acceptDrop(rect, payload)` whose rect is under the mouse takes it and returns true; the drag ends at the end of that frame either way. `isDragging()` and `dragPayload()` let targets show where a drop would go (outline `lastItemRect()` when `mouseIn` it). The dragged widget doesn't count as clicked unless it's released over itself. `beginChild` returns the box's rect, so the empty part of a list can be a drop target after its rows.

### Dropdown lists (popups)

Only one dropdown list is open at a time. The list is drawn in `endDraw`, after everything else, as its own region, so it's on top of the panel and wins hover tests. While it's open, `wantsMouse` is true; a press anywhere outside the list and its button only closes it (the press goes nowhere else). If its dropdown isn't drawn in a frame (e.g. the tab changed), the list closes. A picked option is handed back to the dropdown on the next frame. `setViewport(rect)` sets the area lists are kept inside.

### Undo

Widgets never change a value on the frame they activate. That's what makes undo grouping work: call `trackUndo(ctx)` ([ui_undo.hpp](../../src/application/ui/ui_undo.hpp)) right after a widget, and it calls `history.begin` on activation (before any change) and `history.commit` on release, so a whole drag is one undo step. A press that didn't change anything is cancelled instead. When editing scene data through widgets, edit a copy and write it back after (see `main_panel.cpp`): `history.cancel` replaces the scene's containers, so a reference held across it could dangle.

### Style

All sizes and colors live in `UIStyle`, following the Dracula theme: text `#f8f8f2`, dim text the comment blue `#6272a4`, panels and frames in its blue-grey backgrounds (`#282a36`, `#21222c`, `#44475a`). `ACCENT` is Dracula purple `#bd93f9`, the same as viewport selection, for selection and highlights (selected rows, the active tab, sliders, checkboxes, the radial menu's hovered slice). `ACCENT_GREEN` (`#50fa7b`) marks headings and whatever is being typed into (an edited text field's border and caret, the console's prompt, caret, and input line). `ERROR` is Dracula red, `WARNING` Dracula orange (`#ffb86c`, files an export will replace). X/Y/Z use the grid's axis colors: red, green, and cyan.

### Modal windows

`ui.beginModal(name, viewport, width, height, title)` … `ui.endModal()` draws a window centered in `viewport` over a dimmed backdrop (`MODAL_BACKDROP`), with a header carrying the title in green. The backdrop is a region covering the whole viewport, so drawn last (after every panel) it's the topmost region under the mouse: nothing below can be hovered or clicked, and the viewport doesn't get the mouse. Widgets inside are laid out as in a panel and only respond inside the window. The window doesn't move or resize; `height` includes the header. The app's modal windows are in [application.md](application.md#modal-windows).

### Floating panel

`ui.beginPanel(name, state, bounds, title)` … `ui.endPanel()` wraps widgets in a floating panel:

- `UIPanelState` (just a `Rect`) is owned by the app and kept between frames; the main panel's lives in `ctx.viewport.panel`.
- Draws a soft drop shadow, a rounded background with a 1 px border, and a 32 px header strip (`PANEL_HEADER_HEIGHT`) with the title. The header lightens on hover and while dragging.
- **Dragging:** press on the header and move. The offset where you grabbed is remembered (`m_panelGrabOffset`), so the panel doesn't jump.
- **Resizing:** drag any edge or corner. The grab zone is `PANEL_RESIZE_GRIP` (5 px) on each side of the border, so edges can be grabbed from just outside the panel; the region is widened by that much to make it hoverable. The top edge wins over the header drag. The opposite edges stay put, the size never goes below `PANEL_MIN_WIDTH` × `PANEL_MIN_HEIGHT` (240 × 160), and dragged edges stop at the bounds. The border turns purple while an edge is hovered or being dragged.
- **Cursor:** `ui.cursor()` reports the cursor the UI wants (`UICursor`: arrow, horizontal, vertical, one of two diagonals, or `Text` over a text field) based on the hovered or dragged edges; it stays a resize cursor for the whole drag. The main loop passes it to `Window::setCursor` after each frame.
- **Kept inside `bounds`** on every frame, not just while dragging: a smaller window first shrinks the panel to fit, then moves it inside.
- The whole panel is a region (blocks the viewport); widgets are laid out below the header.
- **Scrolling:** content is clipped to the area under the header. `endPanel` measures how tall the content was (`state.contentHeight`); if it's taller than the visible area, the mouse wheel over the panel scrolls it (`SCROLL_STEP`, 48 px per notch) unless a child box under the mouse took the wheel first, clamped between the top and the bottom. Scrolling uses last frame's height, so it follows content as sections appear or disappear. While the panel content clip is active, widgets only respond when the mouse is inside it, so rows scrolled under the header or past the bottom can't be clicked.
- **Scrollbar:** shown only when content overflows: a faint track and a thumb sized to the visible fraction (at least `SCROLLBAR_MIN_THUMB`), `SCROLLBAR_WIDTH` wide in the right padding, `SCROLLBAR_INSET` from the edge so it doesn't collide with the resize grip. The thumb can be dragged (its hit area is 4 px wider each side) and brightens on hover.
- `UIPanelState` also keeps `activeTab`, `scroll`, and `contentHeight` between frames.
- **Tabs:** `beginPanel(name, state, bounds, tabs)` (a list of names instead of a title) draws tabs left to right in the header, sized to their text; when they'd run past the panel, their padding shrinks (to `MIN_TAB_PADDING` at least) so they still fit. Pressing a tab switches `state.activeTab` (and resets the scroll); the header, tabs included, still drags the panel, so you can grab a tab and move. The selected tab takes the body's color with a 2 px purple underline; the others are dim until hovered. The caller draws the active tab's content.

[main_panel.cpp](../../src/application/ui/main_panel.cpp) is the app's panel: 360 px wide at the top right on first use, bounded by the viewport above the status bar, toggled with `ui panel`, with Objects, Materials, Lights, and Images tabs (see [application.md](application.md)). Its list headers shrink the picker before the title when the panel is narrow.

## Coordinates and types

Pixels, origin at the **top-left**, y pointing **down**.

| Type | Description |
|---|---|
| `Rect { x, y, width, height }` | `right()`, `bottom()`, `center()`, `contains(point)`, `Rect::intersect(a, b)` |
| `Color { r, g, b, a }` | 0..1 floats. `packed()` turns it into RGBA bytes for the vertex. |
| `UIFont { id, glyphWidth, glyphHeight }` | Which font texture to use and its cell size. Get one with `makeUIFont(FontId, ctx.fonts.get(FontId))`. |
| `measureText(font, text)` | Width and height of monospaced text; `\n` starts a new line. Use it for layout (centering, sizing tabs). |
| `fitText(font, text, width)` | The text, or as much as fits in `width` followed by `...` (just `...`, or nothing, when even that doesn't fit). |

## UIDrawList

Besides rects, rounded rects, shadows, lines, ring slices, and text: `gradientRect(rect, topLeft, topRight, bottomRight, bottomLeft)` (each corner its own color, blended across; the shader interpolates the fill) and `image(rect, texture, alpha)` (a renderer texture, premultiplied, as render targets are; the shader divides the alpha back out).

| Method | Description |
|---|---|
| `rect(rect, fill)` | Filled rectangle. |
| `roundedRect(rect, radius, fill, border = {}, borderWidth = 0)` | Rounded rectangle with an optional inside border. Radius is clamped to half the shorter side. |
| `shadow(rect, radius, blur, color)` | Soft shadow shaped like a rounded rect, fading over `blur` pixels on each side of its edge. Draw it before the panel it belongs to, usually offset a few pixels down. |
| `line(start, end, width, color)` | Anti-aliased line of any width (drawn as a thin rotated rectangle, so no `glLineWidth` limits). |
| `ringSlice(center, innerRadius, outerRadius, angle, halfAngle, gap, fill, border = {}, borderWidth = 0)` | A wedge of a ring centered on `angle` (radians, counterclockwise from right), with `gap` pixels of space between it and its neighbors. One quad covering the outer circle. |
| `text(position, text, font, color)` | One quad per character from the top-left `position`, snapped to whole pixels so glyphs stay crisp. Spaces advance without drawing; `\n` starts a new line. |
| `pushClip(rect)` / `popClip()` | Clip everything added until the matching pop. Nested clips intersect. |
| `clear()` | Empties the list; called once per frame. |

Items draw in the order they're added: later items cover earlier ones.

### Batches

Every item becomes 4 vertices and 6 indices. Consecutive items share a `UIDrawBatch` (one draw call) until the clip rect changes, text needs a different font texture than the batch already uses, or an image (`image(rect, texture, alpha)`, a renderer texture such as a material swatch, in `MODE_IMAGE`) needs its texture: a batch binds one font or one image (`UIDrawBatch::image`). Shapes don't need a texture, so shapes and text in one font mix freely: a whole panel is usually 1–3 batches.

### UIVertex

```cpp
struct UIVertex {
    Vec2 position;      // pixels
    Vec2 uv;            // font atlas, glyphs only
    Vec2 local;         // offset from the shape's center in the shape's own axes
    Vec2 halfSize;      // shape half extents
    f32 radius, borderWidth, blur, mode;   // mode: MODE_SHAPE, MODE_GLYPH, MODE_RING_SLICE, or MODE_IMAGE
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
- **Ring slices:** the fields are reused: `halfSize` is (inner, outer) radius, `radius` the half-angle, `blur` the gap. `local` is in the slice's frame (pointing along +x), so the distance is the larger of the radial distance and the distance past either side line. Edges and borders are shaded like shapes.

[radial_layout.hpp](../../src/ui/radial_layout.hpp) has the slice math the radial menu uses: `sliceAngle(slice, count)` and `sliceDirection(slice, count)` (count equal slices, slice 0 centered straight up, the rest clockwise on screen), and `sliceAt(offset, deadZone, count)` (the slice an offset points into, or −1 inside the dead zone).

`OpenGLUIRenderer::draw` turns off depth testing and writes, turns on alpha blending, sets `glScissor` per clipped batch (converting y-down clip rects to GL's y-up), binds the batch's font texture, and restores 3D state afterward.

## Fonts

[font_library.hpp](../../src/core/font/font_library.hpp) loads every embedded font; `ctx.fonts` holds them and `setupRenderer` passes them to `renderer->loadFonts`, which builds one atlas texture per font.

| `FontId` | File | Cell | Used for |
|---|---|---|---|
| `Console` | `assets/fonts/console.bmf` | 16×24 | Console, world-space debug labels |
| `UI` | `assets/fonts/ui.bmf` | 10×16 | Panels, status line |

Glyphs are stored in a 16 × 6 atlas grid in character order, so UVs come from `FontAtlas::glyphUV(char)` ([font_atlas.hpp](../../src/core/font/font_atlas.hpp)) without knowing the glyph size. Characters outside ASCII 32–126 draw as spaces.

`ui.bmf` is generated from `console.bmf`: each 16×24 cell is trimmed to its 14×22 inked area plus a 1 px margin, then area-averaged down to 10×16, so it's the same typeface at a smaller size. Regenerate it after changing the console font with `python tools/scale_bmf.py assets/fonts/console.bmf assets/fonts/ui.bmf 10 16 1 1 15 23`. To add a font: generate a `.bmf`, embed it in `embedded_fonts.cpp`, add a `FontId`, and load it in `FontLibrary::loadEmbedded`.
