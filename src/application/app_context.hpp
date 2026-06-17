#pragma once

#include <vector>
#include <memory>

#include "application/app_systems.hpp"
#include "platform/window/window.hpp"
#include "renderer/renderer.hpp"
#include "core/scene/camera.hpp"
#include "core/scene/render_object.hpp"
#include "core/assets/assets.hpp"

struct AppContext {
    // Settings settings;
    // Project project;

    Systems systems;
    std::unique_ptr<IRenderer> renderer;
    std::vector<std::unique_ptr<Window>> windows;

    Assets assets;

    Camera camera;
    
    RenderObject testObject;

    bool is_running = false;
};