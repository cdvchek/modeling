#pragma once

#include "application/app_context.hpp"

// The floating panel; bounds is the area it must stay inside (the viewport above the status bar)
void drawMainPanel(AppContext& ctx, const Rect& bounds);
