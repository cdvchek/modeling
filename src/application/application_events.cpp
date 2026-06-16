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

    ctx.systems.events.subscribe<Event::Quit>(
        [&ctx](const Event::Quit&) -> bool {
            ctx.is_running = false;
            return true;
        }
    );
}