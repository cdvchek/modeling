#include "application/action_checks/action_checks.hpp"
#include "core/math/vec2.hpp"
#include <iostream>
#include <cmath>

void checkScaleContext(AppContext& ctx) {
    // figure out the center of the window
    u32 width, height;
    ctx.windows[0].get()->getDimensions(width, height);
    u32 centerX = width / 2;
    u32 centerY = height / 2;

    // figure out if the mouse is moving towards the center of the window or away from the center
    i32 mouseX = ctx.systems.input.getMouseX();
    i32 mouseY = ctx.systems.input.getMouseY();

    f64 scaling = std::hypot((f64)((mouseX + ctx.systems.input.getMouseDeltaX()) - (i32)centerX), (f64)((mouseY + ctx.systems.input.getMouseDeltaY()) - (i32)centerY)) / std::hypot((f64)(mouseX - (i32)centerX), (f64)(mouseY - (i32)centerY));

    // figure out the center of all the selected points
    const auto& verts = ctx.scene.objects.get(0).meshData.getVertices();
    Vec3 center = Vec3(0);
    for(const auto& vertIndex : ctx.scene.selection.getVertexIndices()) {
        Vertex vert = verts[vertIndex];
        center += vert.position;
    }
    center = center / ctx.scene.selection.getVertexIndices().size();

    // move points towards or away from center of points depending on mouse movement
    for(const auto& vertIndex : ctx.scene.selection.getVertexIndices()) {
        Vertex vert = verts[vertIndex];
        Vec3 scaleDir = vert.position - center;
        Vec3 newPos = center + (scaleDir * scaling);
        ctx.scene.objects.get(0).meshData.positionVertex(vertIndex, newPos);
        ctx.scene.objects.get(0).meshDirty = true;
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::ConfirmScale, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
        ctx.systems.input_ctx.setContext(ctx.systems.input_ctx.getSelectionContext());
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::CancelScale, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
        const auto& starts = ctx.scene.selection.getSelectionStartPositions();
        const auto& selections = ctx.scene.selection.getVertices();

        for (u32 i = 0; i < (u32)selections.size(); ++i) {
            const auto& selection = selections[i];
            const auto& start = starts[i];
            ctx.scene.objects.get(selection.objectIndex).meshData.positionVertex(selection.vertexIndex, start);
        }
        
        ctx.systems.input_ctx.setContext(ctx.systems.input_ctx.getSelectionContext());
    }
}