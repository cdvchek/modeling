#pragma once

#include <types>

class UIDrawList;
struct AppContext;

// CPU timings the app measures itself; the renderer's own numbers come from IRenderer::getStats
struct FrameStats {
    f32 inputMilliseconds = 0.0f;       // input, tools, and actions (checkActions)
    f32 renderMilliseconds = 0.0f;      // building and submitting the frame, until it's handed to the screen
    f32 lastPickMilliseconds = -1.0f;   // the last click's picking; -1 before the first
};

// The stats readout (stats command): frame times, draw calls, primitives, uploads, and the scene's size, top left
void drawStatsOverlay(const AppContext& ctx, UIDrawList& ui);
