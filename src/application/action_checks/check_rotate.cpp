#include "application/action_checks/action_checks.hpp"

#include <cmath>

void checkRotateContext(AppContext& ctx) {
    bool xAxis = ctx.systems.input_ctx.isActive(InputContext_XAxis);
    bool yAxis = ctx.systems.input_ctx.isActive(InputContext_YAxis);
    bool zAxis = ctx.systems.input_ctx.isActive(InputContext_ZAxis);

    static bool wasXAxis = false;
    static bool wasYAxis = false;
    static bool wasZAxis = false;

    const auto& selections = ctx.scene.selection.getVertices();
    const auto& lights = ctx.scene.selection.getLights();
    const auto& lightStarts = ctx.scene.selection.getLightStartDirections();

    if (selections.empty() && lights.empty())
        return;

    // Puts every selected light's direction back to where it was when the rotation started
    const auto restoreLightDirections = [&]() {
        for (u32 i = 0; i < static_cast<u32>(lights.size()) && i < static_cast<u32>(lightStarts.size()); ++i) {
            if (Light* light = ctx.scene.lights.tryGet(lights[i])) light->direction = lightStarts[i];
        }
    };

    const auto& starts =
        ctx.scene.selection.getSelectionStartPositions();

    // Find center of the ORIGINAL selection.
    Vec3 center(0.0f);

    for (const Vec3& start : starts) {
        center += start;
    }

    // Only lights selected: there's no vertex pivot, and lights turn in place anyway
    if (!starts.empty()) {
        center =
            center / static_cast<f32>(starts.size());
    }

    const bool xJustActivated = xAxis && !wasXAxis;
    const bool yJustActivated = yAxis && !wasYAxis;
    const bool zJustActivated = zAxis && !wasZAxis;

    // Restore the original positions when an axis is first selected.
    if (xJustActivated || yJustActivated || zJustActivated) {
        restoreLightDirections();

        for (u32 i = 0;
             i < static_cast<u32>(selections.size());
             ++i) {

            const VertexSelection& selection =
                selections[i];

            const Vec3& start =
                starts[i];

            Object& object =
                ctx.scene.objects.get(
                    selection.objectIndex
                );

            object.meshData.positionVertex(
                selection.vertex,
                start
            );

            object.meshData.setFacesDirtyByVertex(
                selection.vertex
            );

            object.meshDirty = true;
        }
    }

    // Mouse movement left/right determines rotation.
    const i32 mouseDeltaX =
        ctx.systems.input.getMouseDeltaX();

    if (mouseDeltaX != 0) {
        constexpr f32 ROTATION_SPEED = 0.002f;

        const f32 angle =
            static_cast<f32>(mouseDeltaX) *
            ROTATION_SPEED;

        Vec3 axis;

        // Rotate around the selected world axis.
        if (xAxis) {
            axis = Vec3(1.0f, 0.0f, 0.0f);
        }
        else if (yAxis) {
            axis = Vec3(0.0f, 1.0f, 0.0f);
        }
        else if (zAxis) {
            axis = Vec3(0.0f, 0.0f, 1.0f);
        }
        else {
            // Rotate in the plane perpendicular to the camera.
            axis =
                ctx.scene.camera.getForward().normalized();
        }

        const f32 cosAngle =
            std::cos(angle);

        const f32 sinAngle =
            std::sin(angle);

        // Lights turn in place: only their direction rotates
        for (LightHandle handle : lights) {
            Light* light = ctx.scene.lights.tryGet(handle);
            if (!light) continue;

            const Vec3 direction = light->direction;
            const Vec3 rotated =
                direction * cosAngle +
                Vec3::cross(axis, direction) * sinAngle +
                axis * Vec3::dot(axis, direction) * (1.0f - cosAngle);

            light->direction = rotated.normalized();
        }

        for (const VertexSelection& selection : selections) {
            Object& object =
                ctx.scene.objects.get(
                    selection.objectIndex
                );

            const Vec3 vertexPos =
                object.meshData.getVertexPosition(
                    selection.vertex
                );

            const Vec3 relative =
                vertexPos - center;

            // Rodrigues' rotation formula.
            const Vec3 rotated =
                relative * cosAngle +
                Vec3::cross(axis, relative) * sinAngle +
                axis *
                    Vec3::dot(axis, relative) *
                    (1.0f - cosAngle);

            const Vec3 newPos =
                center + rotated;

            object.meshData.positionVertex(
                selection.vertex,
                newPos
            );

            object.meshData.setFacesDirtyByVertex(
                selection.vertex
            );

            object.meshDirty = true;
        }
    }

    // Confirm rotation.
    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::RotateConfirm,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext()
        )) {

        ctx.history.commit();

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

        for (u32 i = 0;
             i < static_cast<u32>(selections.size());
             ++i) {

            const VertexSelection& selection =
                selections[i];

            const Vec3& start =
                starts[i];

            Object& object =
                ctx.scene.objects.get(
                    selection.objectIndex
                );

            object.meshData.positionVertex(
                selection.vertex,
                start
            );

            object.meshData.setFacesDirtyByVertex(
                selection.vertex
            );

            object.meshDirty = true;
        }

        restoreLightDirections();

        ctx.history.cancel(ctx.scene);

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    wasXAxis = xAxis;
    wasYAxis = yAxis;
    wasZAxis = zAxis;
}