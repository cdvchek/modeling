#pragma once

#include <vector>
#include <memory>

#include "application/app_systems.hpp"
#include "application/bevel_tool.hpp"
#include "application/viewport_settings.hpp"
#include "platform/window/window.hpp"
#include "renderer/renderer.hpp"
#include "renderer/debug_renderer.hpp"
#include "scene/scene.hpp"
#include "scene/history.hpp"

struct AppContext {
    Systems systems;
    std::unique_ptr<IRenderer> renderer;
    DebugRenderer debug_renderer;
    std::vector<std::unique_ptr<Window>> windows;
    Scene scene;
    BevelTool bevel;
    History history;
    ViewportSettings viewport;

    bool is_running = false;
};