#include "application/application.hpp"
#include "application/action_checks/action_checks.hpp"
#include "application/radial_menu.hpp"

bool Application::checkActions(AppContext& ctx) {
    ContextManager& ictx = ctx.systems.input_ctx;

    if (ctx.systems.actions.isActionDown(Action::Quit, ctx.systems.input, ictx.getContext())) {
        ctx.systems.events.trigger(Event::Quit{});
        return false;
    }

    // While the radial menu is open it takes all other input, so tools and the camera hold still
    if (updateRadialMenu(ctx)) return true;

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::ToggleConsole, ctx.systems.input, ictx.getContext())) {
        ctx.systems.input_ctx.toggleContext(InputContext_Console);

        // Closing collapses what's there; anything printed after this shows open next time
        if (!ictx.isActive(InputContext_Console)) ctx.systems.console.collapseEntries();
        else ctx.consoleView.followNewest = true;
    }

    if (ictx.isActive(InputContext_Console)) {
        checkConsoleContext(ctx);
    }
    
    if (ictx.isActive(InputContext_AnySelection)) {
        checkSelectionContext(ctx);
    }

    // One-shot actions (modes, starting tools, operations, undo, axis locks) run through their handlers
    ctx.systems.actions.dispatch(ctx.systems.input, ictx);

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

    if (ictx.isActive(InputContext_Inset)) {
        checkInsetContext(ctx);
    }

    return true;
}