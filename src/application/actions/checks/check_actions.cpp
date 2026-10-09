#include "application/application.hpp"
#include "application/actions/checks/action_checks.hpp"
#include "application/commands/texture_commands.hpp"
#include "application/uv/uv_editor.hpp"
#include "application/paint/paint_workspace.hpp"
#include "application/ui/radial_menu.hpp"
#include "application/ui/modal_windows.hpp"
#include "application/actions/asset_actions.hpp"
#include "application/viewport/reference_images.hpp"

bool Application::checkActions(AppContext& ctx) {
    ContextManager& ictx = ctx.systems.input_ctx;

    if (ctx.systems.actions.isActionDown(Action::Quit, ctx.systems.input, ictx.getContext())) {
        ctx.systems.events.trigger(Event::Quit{});
        return false;
    }

    // A modal window takes all input: only Enter and Escape reach it as actions, the rest is its own buttons
    if (isModalOpen(ctx)) {
        ctx.systems.actions.dispatch(ctx.systems.input, ictx);
        updateModal(ctx);
        return true;
    }

    if (ctx.importRequested) {
        ctx.importRequested = false;
        importAssets(ctx);
    }

    if (ctx.referenceRequested) {
        ctx.referenceRequested = false;
        chooseReferenceImages(ctx);
    }

    if (ctx.textureRequest.open) {
        ctx.textureRequest.open = false;
        chooseTexture(ctx, ctx.textureRequest.material);
    }

    // A UV grab, scale, or rotate takes all input until it's kept or put back
    if (ctx.uvTool.active()) {
        updateUVTool(ctx);
        return true;
    }

    // The UV editor's own view: pan and zoom with the mouse over it
    if (ctx.workspace.current == Workspace::UV) updateUVEditor(ctx);
    // The paint canvas's, in 2D
    if (ctx.workspace.current == Workspace::Paint) updatePaintCanvas(ctx);

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