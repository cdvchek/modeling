#include "application/application.hpp"
#include "application/action_checks/action_checks.hpp"

void Application::checkActions(AppContext& ctx) {
    ContextManager& ictx = ctx.systems.input_ctx; //input context
    if (ictx.isActive(InputContext_Selection)) {
        checkSelectionContext(ctx);
    }

    if (ictx.isActive(InputContext_Grab)) {
        checkGrabContext(ctx);
    }
}