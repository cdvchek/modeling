#include "application/action_checks/action_checks.hpp"
#include "core/math/vec2.hpp"
#include <iostream>
#include <cmath>

void checkScaleContext(AppContext& ctx) {
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
        static_cast<f64>(mouseX + ctx.systems.input.getMouseDeltaX() - static_cast<i32>(centerX)),
        static_cast<f64>(mouseY + ctx.systems.input.getMouseDeltaY() - static_cast<i32>(centerY))
    );

    if (oldDistance == 0.0) return;

    const f64 scaling = newDistance / oldDistance;
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

    // Scale each selected vertex around the selection center.
    for (const VertexSelection& selection : selections) {
        Object& object = ctx.scene.objects.get(selection.objectIndex);
        const Vec3 vertexPos = object.meshData.getVertexPosition(selection.vertex);

        const Vec3 scaleDir = vertexPos - center;
        const Vec3 newPos = center + scaleDir * static_cast<f32>(scaling);

        object.meshData.positionVertex(selection.vertex, newPos);
        object.meshData.setFacesDirtyByVertex(selection.vertex);
        object.meshDirty = true;
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::ConfirmScale,
        ctx.systems.input,
        ctx.systems.input_ctx.getContext()
    )) {
        ctx.systems.input_ctx.setContext(ctx.systems.input_ctx.getSelectionContext());
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
        Action::CancelScale,
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

        ctx.systems.input_ctx.setContext(ctx.systems.input_ctx.getSelectionContext());
    }
}