#pragma once

#include "application/app_context.hpp"

// The top bar with a tab per workspace; the UV workspace's header, divider, UV editor, and tools; and the Paint
// workspace's header, 2D view, and tools. Each is a UI region, so clicks there never reach the 3D view. Call between
// ui.beginDraw and ui.endDraw.
void drawWorkspaceChrome(AppContext& ctx, const ScreenLayout& layout);
