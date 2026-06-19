#pragma once

#include <vector>
#include <memory>

#include "application/app_systems.hpp"
#include "platform/window/window.hpp"
#include "renderer/renderer.hpp"
#include "scene/scene.hpp"

struct AppContext {
    Systems systems;
    std::unique_ptr<IRenderer> renderer;
    std::vector<std::unique_ptr<Window>> windows;
    Scene scene;

    bool is_running = false;
};