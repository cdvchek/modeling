#include "application/action_checks/action_checks.hpp"

#include <iostream>

void beginInset(AppContext& ctx) {
    Selection& selection = ctx.scene.selection;
    WidthTool& inset = ctx.widthTool;
    const ObjectHandle handle = selection.getActiveObject();
    Object* object = ctx.scene.objects.tryGet(handle);
    if (!object) return;

    // Measured in world space, so the width matches the mouse on scaled objects
    ctx.history.begin(ctx.scene);
    std::vector<FaceHandle> inner;
    const RegionError error = object->meshData.insetRegions(selection.getFaceHandles(), inset.session, inner, ctx.scene.objects.worldMatrix(ctx.scene.selection.getActiveObject()));

    if (error != RegionError::None) {
        ctx.history.cancel(ctx.scene);
        ctx.systems.console.printError(std::string("inset: ") + regionErrorText(error));
        return;
    }

    // The new inner faces become the selection; cancel brings the old one back
    inset.savedSelection = selection;
    selection.clear();

    Vec3 center(0.0f);
    u32 corners = 0;
    for (FaceHandle face : inner) {
        selection.addFace(handle, face);
        for (VertexHandle vertex : object->meshData.getFaceVertices(face)) {
            if (!selection.hasVertex(handle, vertex)) selection.addVertex(handle, vertex);
            center += object->meshData.getVertexPosition(vertex);
            ++corners;
        }
    }

    inset.object = handle;
    inset.pivot = corners > 0 ? center / static_cast<f32>(corners) : center;
    inset.startMouseX = static_cast<f32>(ctx.systems.input.getMouseX());
    inset.startMouseY = static_cast<f32>(ctx.systems.input.getMouseY());

    object->meshDirty = true;
    ctx.systems.input_ctx.setContext(InputContext_Inset);
}

void checkInsetContext(AppContext& ctx) {
    updateWidthTool(ctx, Action::ConfirmInset, Action::CancelInset);
}
