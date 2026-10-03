#pragma once

#include "application/app_context.hpp"

void checkConsoleContext(AppContext& ctx);
void checkSelectionContext(AppContext& ctx);
void checkGrabContext(AppContext& ctx);
void checkScaleContext(AppContext& ctx);
void checkRotateContext(AppContext& ctx);

void beginBevel(AppContext& ctx);
void checkBevelContext(AppContext& ctx);