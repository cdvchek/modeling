#pragma once

#include "application/app_context.hpp"

// The console overlay: a panel docked at the bottom of the viewport with commands, output, and errors above an input line
void drawConsole(AppContext& ctx, UIDrawList& ui, const Rect& viewport);

// Mouse wheel scrolling and clicks that collapse or expand entries; runs while the console is open
void updateConsoleView(AppContext& ctx);
