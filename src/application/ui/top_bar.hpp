#pragma once

#include "application/app_context.hpp"

// The top bar with a tab per workspace, and the UV workspace's divider and UV editor area. Each is a UI region, so
// clicks there never reach the 3D view. Call between ui.beginDraw and ui.endDraw.
void drawWorkspaceChrome(AppContext& ctx, const ScreenLayout& layout);
