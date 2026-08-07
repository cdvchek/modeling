#include "application/action_checks/action_checks.hpp"

#include <algorithm>

void checkGrabContext(AppContext& ctx) {
    i32 dx = ctx.systems.input.getMouseDeltaX();
    i32 dy = ctx.systems.input.getMouseDeltaY();

    dx = std::clamp(dx, -500, 500);
    dy = std::clamp(dy, -500, 500);

    for (VertexSelection vs : ctx.scene.selection.getVertices()) {
        Object& obj = ctx.scene.objects.all()[vs.objectIndex];

        Vec3 right = ctx.scene.camera.getRight();
        Vec3 cameraUp = Vec3::cross(right, ctx.scene.camera.getForward()).normalized();

        f32 vertMoveSpeed = 0.001f * ctx.scene.camera.distance;

        Vec3 vertMove = (right * dx - cameraUp * dy) * vertMoveSpeed;

        obj.meshData.translateVertex(vs.vertexIndex, vertMove);
        obj.meshDirty = true;
        obj.meshData.setFacesDirtyByVertex(vs.vertexIndex);
    }

    auto& selections = ctx.scene.selection.getVertices();
    auto& firstSelection = selections.front();
    u32 firstVertIndex = firstSelection.vertexIndex;
    u32 firstObjIndex = firstSelection.objectIndex;

    Vec3 firstPos = ctx.scene.objects.get(firstObjIndex).meshData.getVertices()[firstVertIndex].position;

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::ConfirmGrab, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
        ctx.systems.input_ctx.setContext(InputContext_Selection);
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::CancelGrab, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
        const auto& starts = ctx.scene.selection.getSelectionStartPositions();

        for (u32 i = 0; i < (u32)selections.size(); ++i) {
            const auto& selection = selections[i];
            const auto& start = starts[i];
            ctx.scene.objects.get(selection.objectIndex).meshData.positionVertex(selection.vertexIndex, start);
        }
        
        ctx.systems.input_ctx.setContext(InputContext_Selection);
    }
}