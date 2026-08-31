#include "application/action_checks/action_checks.hpp"

#include <algorithm>

void checkGrabContext(AppContext& ctx) {
    i32 dx = ctx.systems.input.getMouseDeltaX();
    i32 dy = ctx.systems.input.getMouseDeltaY();

    dx = std::clamp(dx, -500, 500);
    dy = std::clamp(dy, -500, 500);

    const Vec3 right = ctx.scene.camera.getRight();
    const Vec3 cameraUp = Vec3::cross(right, ctx.scene.camera.getForward()).normalized();

    const f32 vertMoveSpeed = 0.001f * ctx.scene.camera.distance;
    const Vec3 vertMove = (right * dx - cameraUp * dy) * vertMoveSpeed;

    for (const VertexSelection& vs : ctx.scene.selection.getVertices()) {
        Object& obj = ctx.scene.objects.get(vs.objectIndex);

        obj.meshData.translateVertex(
            vs.vertex,
            vertMove
        );

        obj.meshDirty = true;

        obj.meshData.setFacesDirtyByVertex(
            vs.vertex
        );
    }

    const auto& selections = ctx.scene.selection.getVertices();

    if (selections.empty()) return;

    const VertexSelection& firstSelection = selections.front();
    const VertexHandle firstVertex = firstSelection.vertex;
    const u32 firstObject = firstSelection.objectIndex;
    const Vec3 firstPos = ctx.scene.objects.get(firstObject).meshData.getVertexPosition(firstVertex);

    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::ConfirmGrab,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::CancelGrab,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        const auto& starts = ctx.scene.selection.getSelectionStartPositions();

        for (u32 i = 0; i < static_cast<u32>(selections.size()); ++i) {
            const VertexSelection& selection = selections[i];
            const Vec3& start = starts[i];

            Object& obj = ctx.scene.objects.get(selection.objectIndex);

            obj.meshData.positionVertex(
                selection.vertex,
                start
            );

            obj.meshData.setFacesDirtyByVertex(
                selection.vertex
            );

            obj.meshDirty = true;
        }

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }
}