#pragma once

#include "application/app_context.hpp"

// Projects the selection center to the screen and records the mouse's start distance and angle; false if it's behind the camera
bool initTransformTool(AppContext& ctx);

// Lines from the scale/rotate pivot or the bevel start point to the mouse
void drawToolGuides(const AppContext& ctx, UIDrawList& ui);
