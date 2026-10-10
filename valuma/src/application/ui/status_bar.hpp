#pragma once

#include "application/app_context.hpp"

const char* selectionModeName(const ContextManager& contexts);
const char* activeToolName(const ContextManager& contexts);
f32 statusBarHeight(const AppContext& ctx);
void drawStatusBar(const AppContext& ctx, UIDrawList& ui, f32 width, f32 height);
