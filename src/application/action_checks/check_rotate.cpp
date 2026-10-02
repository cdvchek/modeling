#include "application/action_checks/action_checks.hpp"

void checkRotateContext(AppContext& ctx) {
    const auto& selections = ctx.scene.selection.getVertices();
    if (selections.empty()) return;

    // Find center of all selected vertices.
    Vec3 center(0.0f);

    for (const VertexSelection& selection : selections) {
        const Object& object = ctx.scene.objects.get(selection.objectIndex);
        const Vec3 vertexPos = object.meshData.getVertexPosition(selection.vertex);

        center += vertexPos;
    }

    center = center / static_cast<f32>(selections.size());

    // Mouse movement left/right determines rotation.
    const i32 mouseDeltaX = ctx.systems.input.getMouseDeltaX();

    if (mouseDeltaX != 0) {
        constexpr f32 ROTATION_SPEED = 0.002f;

        // Mouse right = clockwise.
        // Remove the negative if this feels backwards.
        const f32 angle = static_cast<f32>(mouseDeltaX) * ROTATION_SPEED;

        // Rotation axis is parallel to the camera direction,
        // so the rotation plane is perpendicular to the camera.
        const Vec3 axis = ctx.scene.camera.getForward().normalized();

        const f32 cosAngle = std::cos(angle);
        const f32 sinAngle = std::sin(angle);

        for (const VertexSelection& selection : selections) {
            Object& object = ctx.scene.objects.get(selection.objectIndex);
            const Vec3 vertexPos = object.meshData.getVertexPosition(selection.vertex);

            // Move vertex relative to the selection center.
            const Vec3 relative = vertexPos - center;

            // Rotate around camera direction using
            // Rodrigues' rotation formula.
            const Vec3 rotated =
                relative * cosAngle + Vec3::cross(axis, relative) * sinAngle +
                axis * Vec3::dot(axis, relative) * (1.0f - cosAngle);

            // Move back from pivot-relative coordinates.
            const Vec3 newPos = center + rotated;

            object.meshData.positionVertex(selection.vertex, newPos);
            object.meshData.setFacesDirtyByVertex(selection.vertex);
            object.meshDirty = true;
        }
    }

    // Confirm rotation.
    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::RotateConfirm,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext()
    )) {
        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    // Cancel rotation and restore original positions.
    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::RotateCancel,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext()
    )) {
        const auto& starts = ctx.scene.selection.getSelectionStartPositions();

        for (u32 i = 0; i < static_cast<u32>(selections.size()); ++i) {
            const VertexSelection& selection = selections[i];
            const Vec3& start = starts[i];

            Object& object = ctx.scene.objects.get(selection.objectIndex);

            object.meshData.positionVertex(selection.vertex, start);
            object.meshData.setFacesDirtyByVertex(selection.vertex);
            object.meshDirty = true;
        }

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }
}