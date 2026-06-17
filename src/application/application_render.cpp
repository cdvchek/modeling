#include "application/application.hpp"
#include "platform/window/window.hpp"

bool Application::createMainWindow(AppContext& ctx) {
    ctx.windows.emplace_back(std::make_unique<Window>(1920, 1080));

    if (!ctx.windows[0]->initialize(&ctx.systems.events)) {
        ctx.windows[0]->printWindowError();
        return false;
    }

    return true;
}

bool Application::setupRenderer(AppContext& ctx) {
    ctx.renderer = createRenderer(RendererBackend::RB_OpenGL);

    RendererConfig config{};
    ctx.windows[0]->getDimensions(config.width, config.height);
    config.enableVSync = true;

    if (!ctx.renderer->initialize(ctx.windows[0]->getNativeHandle(), ctx.windows[0]->getNativeDisplayContext(), config)) return false;
    return true;
}