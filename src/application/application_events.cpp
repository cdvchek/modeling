#include "application/application.hpp"

void Application::registerInputEvents(AppContext& ctx) {
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

    ctx.systems.events.subscribe<Event::Char>(
        [&ctx](const Event::Char& event) -> bool {
            if (ctx.systems.input_ctx.getContext() & InputContext_Console) {
                if (event.character != '\b' && event.character != '\r' && event.character != '\t')
                    ctx.systems.console.insertToCurrentCommand(event.character);
            }
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

    ctx.systems.events.subscribe<Event::MouseMove>(
        [&ctx](const Event::MouseMove& event) -> bool {
            ctx.systems.input.onMouseMove(event.x, event.y);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseWheel>(
        [&ctx](const Event::MouseWheel& event) -> bool {
            ctx.systems.input.onScroll(event.delta);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::WindowResize>(
        [&ctx](const Event::WindowResize& event) -> bool {
            // Minimized: nothing to draw into.
            if (event.width == 0 || event.height == 0) return false;

            ctx.renderer->resize(event.width, event.height);

            // While an edge is being dragged, Windows runs its own message loop
            // and the main loop is paused, so draw here to keep the view live.
            if (ctx.is_running) renderFrame(ctx);

            return false;
        }
    );

    ctx.systems.events.subscribe<Event::Quit>(
        [&ctx](const Event::Quit&) -> bool {
            ctx.is_running = false;
            return true;
        }
    );
}