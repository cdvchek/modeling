#include "application/application.hpp"
#include "application/action_checks/action_checks.hpp"

bool Application::checkActions(AppContext& ctx) {
    ContextManager& ictx = ctx.systems.input_ctx; //input context

    if (ctx.systems.actions.isActionDown(Action::Quit, ctx.systems.input, ictx.getContext())) {
        ctx.systems.events.trigger(Event::Quit{});
        return false;
    }
    
    if (ictx.isActive(InputContext_Selection)) {
        checkSelectionContext(ctx);
    }

    if (ictx.isActive(InputContext_Grab)) {
        checkGrabContext(ctx);
    }

    if (ictx.isActive(InputContext_Scale)) {
        checkScaleContext(ctx);
    }

    if (ictx.isActive(InputContext_Extrude)) {
        checkExtrudeContext(ctx);
    }

    return true;
}