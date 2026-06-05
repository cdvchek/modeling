#pragma once

#include <vector>
#include <memory>

#include "application/app_systems.hpp"
#include "platform/window/window.hpp"
#include "renderer/renderer.hpp"

struct AppContext {
    // Settings settings;
    // Project project;

    Systems systems;
    std::unique_ptr<IRenderer> renderer;
    std::vector<std::unique_ptr<Window>> windows;

    bool is_running = false;
};