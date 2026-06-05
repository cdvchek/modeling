#include "application/application.hpp"
#include "platform/window/window.hpp"
#include "platform/platform.hpp"

#include <iostream>

bool Application::initialize(AppContext& ctx) {
    ctx.windows.emplace_back(std::make_unique<Window>(1920, 1080));

    if (!ctx.windows[0]->initialize(&ctx.systems.events)) {
        ctx.windows[0]->printWindowError();
        return false;
    }

    ctx.renderer = createRenderer(RendererBackend::RB_OpenGL);

    RendererConfig config{};
    ctx.windows[0]->getDimensions(config.width, config.height);
    config.enableVSync = true;

    if (!ctx.renderer->initialize(ctx.windows[0]->getNativeHandle(), ctx.windows[0]->getNativeDisplayContext(), config)) return false;

    ctx.systems.events.subscribe<Event::KeyDown>(
        [&ctx](const Event::KeyDown& event) -> bool {
            ctx.systems.input.onKey(event.key, true);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::KeyUp>(
        [&ctx](const Event::KeyUp& event) -> bool {
            ctx.systems.input.onKey(event.key, false);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseButtonDown>(
        [&ctx](const Event::MouseButtonDown& event) -> bool {
            ctx.systems.input.onMouseButton(event.button, true);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseButtonUp>(
        [&ctx](const Event::MouseButtonUp& event) -> bool {
            ctx.systems.input.onMouseButton(event.button, false);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::Quit>(
        [&ctx](const Event::Quit& event) -> bool {
            ctx.is_running = false;
            return true;
        }
    );

    ctx.systems.actions.subscribe(Action::Quit, DefaultKeybinds::Quit);
    ctx.systems.actions.subscribe(Action::ViewportOrbit, DefaultKeybinds::ViewportOrbit);
    ctx.systems.actions.subscribe(Action::ViewportPan, DefaultKeybinds::ViewportPan);
    // ctx.systems.actions.subscribe(Action::ViewportZoom, DefaultKeybinds::ViewportZoom);
    ctx.systems.actions.subscribe(Action::Select, DefaultKeybinds::Select);
    ctx.systems.actions.subscribe(Action::Insert, DefaultKeybinds::Insert);
    ctx.systems.actions.subscribe(Action::DeleteSelection, DefaultKeybinds::DeleteSelection);
    ctx.systems.actions.subscribe(Action::Duplicate, DefaultKeybinds::Duplicate);
    ctx.systems.actions.subscribe(Action::Connect, DefaultKeybinds::Connect);
    ctx.systems.actions.subscribe(Action::Disconnect, DefaultKeybinds::Disconnect);
    ctx.systems.actions.subscribe(Action::Save, DefaultKeybinds::Save);
    ctx.systems.actions.subscribe(Action::Undo, DefaultKeybinds::Undo);
    ctx.systems.actions.subscribe(Action::Redo, DefaultKeybinds::Redo);


    ctx.is_running = false;

    return true;
}

void Application::run(AppContext& ctx) {
    ctx.is_running = true;

    while(ctx.is_running) {
        ctx.systems.input.beginFrame();
        
        Platform::pollEvents();
        
        if (ctx.systems.actions.isActionDown(Action::Quit, ctx.systems.input)) {
            ctx.systems.events.trigger(Event::Quit{});
            break;
        }

        Application::renderFrame(ctx);
    }
} 

void Application::renderFrame(AppContext& ctx) {
    ctx.renderer->beginFrame();
    ctx.renderer->beginMainPass(ctx.renderer->m_clearState);
    
    DrawCommand cmd;
    ctx.renderer->draw(cmd);

    // for (const DrawCommand& cmd : ctx.rendererDrawCommands) {
    //     ctx.renderer->draw(cmd);
    // }

    ctx.renderer->endMainPass();
    ctx.renderer->endFrame();
    ctx.renderer->present();
}