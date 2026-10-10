#pragma once

#include "application/app_context.hpp"

// The UV editor: the texture flat, with the UVs of the object being UV-edited drawn over it. UV space runs with u to
// the right and v down, so (0, 0) is the texture's top-left corner and the picture shows the right way up.

// UV space to window pixels inside area, and back, for a view
Vec2 uvToScreen(const UVView& view, const Rect& area, Vec2 uv);
Vec2 screenToUV(const UVView& view, const Rect& area, Vec2 screen);

// Panning and zooming a flat view with the mouse over area (and nothing else owning the mouse): a middle or right
// drag pans (and keeps going wherever the mouse goes), the wheel zooms toward the cursor. True when the mouse is over
// area and free.
bool navigateUVView(AppContext& ctx, UVView& view, const Rect& area);

// A flat view's background: the 0 to 1 square with the texture (0: a checker), the grid if wanted, and its outline
void drawTextureSquare(UIDrawList& list, const UVView& view, const Rect& area, u32 texture, bool grid);

// Fits the UV rectangle low to high into area at 90%
void fitUVView(UVView& view, const Rect& area, Vec2 low, Vec2 high);

// With the mouse over the UV editor: the middle (or right) button drags the view, the wheel zooms toward the cursor.
// Call from checkActions in the UV workspace.
void updateUVEditor(AppContext& ctx);

// Draws the editor into area (inside a UI region): the 0 to 1 square with the background texture, the grid, and
// every face's UVs as a wireframe. A view that hasn't been framed yet (zoom 0) frames everything first.
void drawUVEditor(AppContext& ctx, const Rect& area);

// The texture behind the UVs: the chosen one, or the material's base color map of the selected faces (the first
// selected face's own material, else the object's); 0 for the checker
u32 uvBackgroundTexture(const AppContext& ctx);

// Fits the selected UVs (the selected faces' corners, or every corner at a selected vertex), or with nothing
// selected or all set every UV and the 0 to 1 square, into the UV editor
void frameUVs(AppContext& ctx, bool all);
// Points the camera at the selected vertices, or with nothing selected or all set the whole object, so they fill
// the 3D view
void frameScene(AppContext& ctx, bool all);
// F and A: frames whichever view the mouse is over (Paint: its one view)
void frameView(AppContext& ctx, bool all);

// Ctrl+A: every vertex, edge, or face of the object being UV-edited, by mode
void selectAllUVs(AppContext& ctx);
