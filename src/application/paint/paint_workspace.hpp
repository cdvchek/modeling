#pragma once

#include "application/app_context.hpp"

// The Paint workspace: one viewport showing the object being painted in 3D, or (Tab) its active texture flat with the
// UVs over it in 2D, and a tools column. Faces whose material's base map isn't the active texture are dimmed in both.

// The texture being painted: the chosen one while the object's materials use it, else the first they use; invalid
// when they use none
TextureHandle activePaintTexture(const AppContext& ctx);

// Tab: the model (3D) or the flat texture (2D)
void togglePaintView(AppContext& ctx);

// Alt+click in 3D: the clicked face's texture becomes the active one (a face without one says how to make one)
bool canPickPaintTexture(const AppContext& ctx);
void pickPaintTexture(AppContext& ctx);

// With the mouse over the 2D view: a middle or right drag pans, the wheel zooms. Call from checkActions in Paint.
void updatePaintCanvas(AppContext& ctx);

// The 2D view (inside a UI region): the active texture, paintable faces' UVs as a wireframe, other faces shaded over
// and faint. A view that hasn't been framed yet frames the texture first.
void drawPaintCanvas(AppContext& ctx, const Rect& area);

// F and A in 2D: the paintable faces' UVs (or with none, or all set, the whole texture)
void framePaintCanvas(AppContext& ctx, bool all);

// The tools column: what's being painted
void drawPaintToolsPanel(AppContext& ctx, const Rect& area);

// New texture for a material: asks for a size, then makes a texture of the material's base color, gives it to the
// material as its base map, and sets the base color to white so nothing changes on screen; one undo step
void openNewTextureWindow(AppContext& ctx, MaterialHandle material);
TextureHandle createSolidTexture(AppContext& ctx, MaterialHandle material, u32 width, u32 height);
void drawNewTextureWindow(AppContext& ctx, const Rect& viewport);
void updateNewTextureWindow(AppContext& ctx);
void confirmNewTextureWindow(AppContext& ctx);

// "1024 x 1024"
std::string textureSizeText(const Texture& texture);
