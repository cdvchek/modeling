#include "application/action_checks/action_checks.hpp"

#include <algorithm>

void checkGrabContext(AppContext& ctx) {
    for (VertexSelection vs : ctx.scene.selection.getVertices()) {
        Object& obj = ctx.scene.objects.all()[vs.objectIndex];

        i32 dx = ctx.systems.input.getMouseDeltaX();
        i32 dy = ctx.systems.input.getMouseDeltaY();

        dx = std::clamp(dx, -500, 500);
        dy = std::clamp(dy, -500, 500);

        Vec3 right = ctx.scene.camera.getRight();
        Vec3 cameraUp = Vec3::cross(right, ctx.scene.camera.getForward()).normalized();

        f32 vertMoveSpeed = 0.001f * ctx.scene.camera.distance;

        Vec3 vertMove = (right * dx - cameraUp * dy) * vertMoveSpeed;

        obj.meshData.translateVertex(vs.vertexIndex, vertMove);
        obj.meshDirty = true;
    }

    auto& selections = ctx.scene.selection.getVertices();
    auto& firstSelection = selections.front();
    u32 firstVertIndex = firstSelection.vertexIndex;
    u32 firstObjIndex = firstSelection.objectIndex;

    Vec3 firstPos = ctx.scene.objects.get(firstObjIndex).meshData.getVertices()[firstVertIndex].position;

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::ConfirmGrab, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
        ctx.scene.selection.setSelectionStartPosition(firstPos);
        ctx.systems.input_ctx.setContext(InputContext_Selection);
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::CancelGrab, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
        Vec3 delta = ctx.scene.selection.getSelectionDelta(firstPos);

        for (auto selection : selections) {
            auto& meshData = ctx.scene.objects.get(selection.objectIndex).meshData;
            Vec3 currentPos = meshData.getVertices()[selection.vertexIndex].position;
            Vec3 newPos = currentPos - delta;
            ctx.scene.objects.get(selection.objectIndex).meshData.moveVertex(selection.vertexIndex, newPos);
        }
        
        ctx.systems.input_ctx.setContext(InputContext_Selection);
    }
}