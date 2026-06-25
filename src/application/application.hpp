#pragma once

#include "application/app_context.hpp"

namespace Application {
    bool initialize(AppContext& ctx);
    void run(AppContext& ctx);
    void renderFrame(AppContext& ctx);
    
    bool createMainWindow(AppContext& ctx);
    bool setupRenderer(AppContext& ctx);

    void registerInputEvents(AppContext& ctx);
    void registerDefaultActions(AppContext& ctx);

    void checkActions(AppContext& ctx);

    void initializeCamera(AppContext& ctx);
    void loadTestScene(AppContext& ctx);
}