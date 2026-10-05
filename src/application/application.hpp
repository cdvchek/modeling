#pragma once

#include "application/app_context.hpp"

namespace Application {
    bool initialize(AppContext& ctx);
    void run(AppContext& ctx);
    void renderFrame(AppContext& ctx);
    void drawConsole(AppContext& ctx, UIDrawList& ui, f32 width, f32 height);
    
    bool createMainWindow(AppContext& ctx);
    bool setupRenderer(AppContext& ctx);

    void registerInputEvents(AppContext& ctx);
    void registerDefaultActions(AppContext& ctx);
    void registerCommands(AppContext& ctx);

    bool checkActions(AppContext& ctx);

    void initializeCamera(AppContext& ctx);
    void loadTestScene(AppContext& ctx);
}