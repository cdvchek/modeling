#pragma once

#include "application/app_context.hpp"

void checkConsoleContext(AppContext& ctx);
void checkSelectionContext(AppContext& ctx);
void checkGrabContext(AppContext& ctx);
void checkScaleContext(AppContext& ctx);
void checkRotateContext(AppContext& ctx);

void beginBevel(AppContext& ctx);
void checkBevelContext(AppContext& ctx);

// Inset every selected face region by a width that follows the mouse, like bevel
void beginInset(AppContext& ctx);
void checkInsetContext(AppContext& ctx);

// Shared by bevel and inset: width from the mouse, then confirm (keep) or cancel (restore the mesh and selection)
void updateWidthTool(AppContext& ctx, Action confirm, Action cancel);