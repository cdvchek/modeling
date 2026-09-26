#include "application/application.hpp"
#include "application/action_checks/action_checks.hpp"

bool Application::checkActions(AppContext& ctx) {
    ContextManager& ictx = ctx.systems.input_ctx; //input context

    if (ctx.systems.actions.isActionDown(Action::Quit, ctx.systems.input, ictx.getContext())) {
        ctx.systems.events.trigger(Event::Quit{});
        return false;
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::ToggleConsole, ctx.systems.input, ictx.getContext())) {
        ctx.systems.input_ctx.toggleContext(InputContext_Console);
    }

    if (ictx.isActive(InputContext_Console)) {
        checkConsoleContext(ctx);
    }
    
    if (ictx.isActive(InputContext_SelectionVertex | InputContext_SelectionEdge | InputContext_SelectionFace)) {
        checkSelectionContext(ctx);
    }

    if (ictx.isActive(InputContext_Grab)) {
        checkGrabContext(ctx);
    }

    if (ictx.isActive(InputContext_Scale)) {
        checkScaleContext(ctx);
    }

    return true;
}