#include "application/application.hpp"
#include "application/project_actions.hpp"
#include "application/modal_windows.hpp"

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
            ctx.systems.input.onChar(event.character);

            if (ctx.systems.input_ctx.getContext() & InputContext_Console) {
                // '/' opens and closes the console, so it never ends up in the command
                if (event.character != '\b' && event.character != '\r' && event.character != '\t' && event.character != '/')
                    ctx.systems.console.insertToCurrentCommand(event.character);
            }
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseButtonDown>(
        [&ctx](const Event::MouseButtonDown& event) -> bool {
            // The click's own position: the move to it can arrive after the click (e.g. after the cursor jumps)
            ctx.systems.input.onMouseMove(event.x, event.y);
            ctx.systems.input.onMouseButton(event.button, true);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseButtonUp>(
        [&ctx](const Event::MouseButtonUp& event) -> bool {
            ctx.systems.input.onMouseMove(event.x, event.y);
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
            if (event.width == 0 || event.height == 0) return false;

            ctx.renderer->resize(event.width, event.height);

            // Windows pauses the main loop while resizing, so draw here.
            if (ctx.is_running) renderFrame(ctx);

            return false;
        }
    );

    ctx.systems.events.subscribe<Event::Quit>(
        [&ctx](const Event::Quit&) -> bool {
            // Closing with unsaved changes asks first (once, however often Alt+F4 repeats); Cancel keeps the app open
            if (ctx.modal.kind == ModalKind::Prompt) return true;
            confirmDiscardChanges(ctx, [&ctx] { ctx.is_running = false; });
            return true;
        }
    );
}