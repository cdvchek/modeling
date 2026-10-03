#include "application/application.hpp"
#include "application/action_checks/action_checks.hpp"

bool Application::checkActions(AppContext& ctx) {
    ContextManager& ictx = ctx.systems.input_ctx;

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

    if (ictx.isActive(InputContext_Grab | InputContext_Scale | InputContext_Rotate)) {
        if (ctx.systems.actions.wasActionPressedThisFrame(Action::XAxis, ctx.systems.input, ictx.getContext())) {
            ictx.toggleContext(InputContext_XAxis);
        }

        if (ctx.systems.actions.wasActionPressedThisFrame(Action::YAxis, ctx.systems.input, ictx.getContext())) {
            ictx.toggleContext(InputContext_YAxis);
        }

        if (ctx.systems.actions.wasActionPressedThisFrame(Action::ZAxis, ctx.systems.input, ictx.getContext())) {
            ictx.toggleContext(InputContext_ZAxis);
        }
    }

    if (ictx.isActive(InputContext_Grab)) {
        checkGrabContext(ctx);
    }

    if (ictx.isActive(InputContext_Scale)) {
        checkScaleContext(ctx);
    }

    if (ictx.isActive(InputContext_Rotate)) {
        checkRotateContext(ctx);
    }

    if (ictx.isActive(InputContext_Bevel)) {
        checkBevelContext(ctx);
    }

    return true;
}