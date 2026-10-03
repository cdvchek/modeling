#include "application/action_checks/action_checks.hpp"

#include <algorithm>

#include <iostream>

void checkGrabContext(AppContext& ctx) {
    bool xAxis = ctx.systems.input_ctx.isActive(InputContext_XAxis);
    bool yAxis = ctx.systems.input_ctx.isActive(InputContext_YAxis);
    bool zAxis = ctx.systems.input_ctx.isActive(InputContext_ZAxis);

    static bool wasXAxis = false;
    static bool wasYAxis = false;
    static bool wasZAxis = false;

    const auto& selections = ctx.scene.selection.getVertices();

    if (selections.empty()) return;

    const auto& starts =
        ctx.scene.selection.getSelectionStartPositions();

    // Check if an axis was just activated.
    const bool xJustActivated = xAxis && !wasXAxis;
    const bool yJustActivated = yAxis && !wasYAxis;
    const bool zJustActivated = zAxis && !wasZAxis;

    // Snap vertices back onto the selected axis.
    if (xJustActivated || yJustActivated || zJustActivated) {
        for (u32 i = 0; i < static_cast<u32>(selections.size()); ++i) {
            const VertexSelection& selection = selections[i];
            const Vec3& start = starts[i];

            Object& obj =
                ctx.scene.objects.get(selection.objectIndex);

            Vec3 currPos =
                obj.meshData.getVertexPosition(selection.vertex);

            Vec3 newPos = currPos;

            if (xJustActivated) {
                newPos.y = start.y;
                newPos.z = start.z;
            }
            else if (yJustActivated) {
                newPos.x = start.x;
                newPos.z = start.z;
            }
            else if (zJustActivated) {
                newPos.x = start.x;
                newPos.y = start.y;
            }

            obj.meshData.positionVertex(
                selection.vertex,
                newPos
            );

            obj.meshDirty = true;

            obj.meshData.setFacesDirtyByVertex(
                selection.vertex
            );
        }
    }

    i32 dx = ctx.systems.input.getMouseDeltaX();
    i32 dy = ctx.systems.input.getMouseDeltaY();

    dx = std::clamp(dx, -500, 500);
    dy = std::clamp(dy, -500, 500);

    const Vec3 right = ctx.scene.camera.getRight();

    const Vec3 cameraUp =
        Vec3::cross(
            right,
            ctx.scene.camera.getForward()
        ).normalized();

    const f32 vertMoveSpeed =
        0.001f * ctx.scene.camera.distance;

    Vec3 vertMove =
        (right * dx - cameraUp * dy) * vertMoveSpeed;

    // Restrict movement to the selected axis.
    if (xAxis || yAxis || zAxis) {
        if (!xAxis) vertMove.x = 0.0f;
        if (!yAxis) vertMove.y = 0.0f;
        if (!zAxis) vertMove.z = 0.0f;
    }

    // Move selected vertices.
    for (const VertexSelection& vs : selections) {
        Object& obj =
            ctx.scene.objects.get(vs.objectIndex);

        obj.meshData.translateVertex(
            vs.vertex,
            vertMove
        );

        obj.meshDirty = true;

        obj.meshData.setFacesDirtyByVertex(
            vs.vertex
        );
    }

    // Confirm grab.
    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::ConfirmGrab,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        ctx.history.commit();

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    // Cancel grab.
    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::CancelGrab,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        for (u32 i = 0;
             i < static_cast<u32>(selections.size());
             ++i) {

            const VertexSelection& selection = selections[i];
            const Vec3& start = starts[i];

            Object& obj =
                ctx.scene.objects.get(selection.objectIndex);

            obj.meshData.positionVertex(
                selection.vertex,
                start
            );

            obj.meshData.setFacesDirtyByVertex(
                selection.vertex
            );

            obj.meshDirty = true;
        }

        ctx.history.cancel(ctx.scene);

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    wasXAxis = xAxis;
    wasYAxis = yAxis;
    wasZAxis = zAxis;
}