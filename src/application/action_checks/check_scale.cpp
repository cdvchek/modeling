#include "application/action_checks/action_checks.hpp"
#include "core/math/vec2.hpp"

#include <iostream>
#include <cmath>

void checkScaleContext(AppContext& ctx) {
    bool xAxis = ctx.systems.input_ctx.isActive(InputContext_XAxis);
    bool yAxis = ctx.systems.input_ctx.isActive(InputContext_YAxis);
    bool zAxis = ctx.systems.input_ctx.isActive(InputContext_ZAxis);

    static bool wasXAxis = false;
    static bool wasYAxis = false;
    static bool wasZAxis = false;

    u32 width, height;
    ctx.windows[0].get()->getDimensions(width, height);

    const u32 centerX = width / 2;
    const u32 centerY = height / 2;

    const i32 mouseX = ctx.systems.input.getMouseX();
    const i32 mouseY = ctx.systems.input.getMouseY();

    const f64 oldDistance = std::hypot(
        static_cast<f64>(mouseX - static_cast<i32>(centerX)),
        static_cast<f64>(mouseY - static_cast<i32>(centerY))
    );

    const f64 newDistance = std::hypot(
        static_cast<f64>(
            mouseX +
            ctx.systems.input.getMouseDeltaX() -
            static_cast<i32>(centerX)
        ),
        static_cast<f64>(
            mouseY +
            ctx.systems.input.getMouseDeltaY() -
            static_cast<i32>(centerY)
        )
    );

    if (oldDistance == 0.0)
        return;

    const f64 scaling = newDistance / oldDistance;

    const auto& selections = ctx.scene.selection.getVertices();

    if (selections.empty())
        return;

    const auto& starts =
        ctx.scene.selection.getSelectionStartPositions();

    // Find center of the ORIGINAL selection.
    Vec3 center(0.0f);

    for (const Vec3& start : starts) {
        center += start;
    }

    center =
        center / static_cast<f32>(starts.size());

    const bool xJustActivated = xAxis && !wasXAxis;
    const bool yJustActivated = yAxis && !wasYAxis;
    const bool zJustActivated = zAxis && !wasZAxis;

    // If an axis was just selected, restore the other axes.
    if (xJustActivated || yJustActivated || zJustActivated) {
        for (u32 i = 0;
             i < static_cast<u32>(selections.size());
             ++i) {

            const VertexSelection& selection =
                selections[i];

            const Vec3& start =
                starts[i];

            Object& object =
                ctx.scene.objects.get(selection.objectIndex);

            Vec3 currentPos =
                object.meshData.getVertexPosition(
                    selection.vertex
                );

            Vec3 newPos = currentPos;

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

    // Scale each selected vertex around the selection center.
    for (const VertexSelection& selection : selections) {
        Object& object =
            ctx.scene.objects.get(selection.objectIndex);

        const Vec3 vertexPos =
            object.meshData.getVertexPosition(
                selection.vertex
            );

        Vec3 scaleDir =
            vertexPos - center;

        // Restrict scaling to the selected axis.
        if (xAxis || yAxis || zAxis) {
            if (!xAxis)
                scaleDir.x = 0.0f;

            if (!yAxis)
                scaleDir.y = 0.0f;

            if (!zAxis)
                scaleDir.z = 0.0f;
        }

        Vec3 newPos;

        if (xAxis || yAxis || zAxis) {
            // Apply only the change caused by scaling.
            Vec3 scaleChange =
                scaleDir *
                (static_cast<f32>(scaling) - 1.0f);

            newPos =
                vertexPos + scaleChange;
        }
        else {
            // Normal scaling on all axes.
            newPos =
                center +
                scaleDir * static_cast<f32>(scaling);
        }

        object.meshData.positionVertex(
            selection.vertex,
            newPos
        );

        object.meshData.setFacesDirtyByVertex(
            selection.vertex
        );

        object.meshDirty = true;
    }

    // Confirm scale.
    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::ConfirmScale,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext()
        )) {

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    // Cancel scale.
    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::CancelScale,
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

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    wasXAxis = xAxis;
    wasYAxis = yAxis;
    wasZAxis = zAxis;
}