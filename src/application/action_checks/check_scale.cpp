#include "application/action_checks/action_checks.hpp"
#include "application/tool_guides.hpp"
#include "core/math/vec2.hpp"

#include <iostream>
#include <cmath>

namespace {
    constexpr f32 MIN_OBJECT_SCALE = 0.001f;
}

void checkScaleContext(AppContext& ctx) {
    const bool xAxis = ctx.systems.input_ctx.isActive(InputContext_XAxis);
    const bool yAxis = ctx.systems.input_ctx.isActive(InputContext_YAxis);
    const bool zAxis = ctx.systems.input_ctx.isActive(InputContext_ZAxis);
    const bool locked = xAxis || yAxis || zAxis;

    const auto& selections = ctx.scene.selection.getVertices();
    const auto& starts = ctx.scene.selection.getSelectionStartPositions();
    const auto& objects = ctx.scene.selection.getObjects();
    const auto& objectStarts = ctx.scene.selection.getObjectStartTransforms();

    if ((selections.empty() && objects.empty()) || !initTransformTool(ctx)) return;

    // Scales the locked components (all of them when nothing is locked) and keeps the rest
    const auto applyLock = [&](Vec3 scaled, Vec3 start) {
        if (!locked) return scaled;
        return Vec3(xAxis ? scaled.x : start.x, yAxis ? scaled.y : start.y, zAxis ? scaled.z : start.z);
    };

    // Factor is the mouse's distance from the pivot on screen relative to where it started
    const TransformTool& tool = ctx.transformTool;
    const Vec2 mouse(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    const f32 scaling = (mouse - tool.pivot).length() / tool.startDistance;

    // Every frame starts over from the start positions; locked-out axes keep theirs.
    // Works in world space so axis locks are world axes on any object transform.
    if (!starts.empty() && !selections.empty()) {
        const ObjectSpace space(ctx.scene.objects.get(selections[0].object).transform);

        Vec3 center(0.0f);
        for (const Vec3& start : starts) center += space.pointToWorld(start);
        center = center / static_cast<f32>(starts.size());

        for (u32 i = 0; i < static_cast<u32>(selections.size()); ++i) {
            const VertexSelection& selection = selections[i];
            const Vec3 start = space.pointToWorld(starts[i]);
            Object& object = ctx.scene.objects.get(selection.object);

            object.meshData.positionVertex(selection.vertex, space.pointToLocal(applyLock(center + (start - center) * scaling, start)));
            object.meshData.setFacesDirtyByVertex(selection.vertex);
            object.meshDirty = true;
        }
    }

    // Objects spread out from their shared center and grow by the same factor
    if (!objectStarts.empty()) {
        Vec3 center(0.0f);
        for (const Transform& start : objectStarts) center += start.position;
        center = center / static_cast<f32>(objectStarts.size());

        for (u32 i = 0; i < static_cast<u32>(objects.size()) && i < static_cast<u32>(objectStarts.size()); ++i) {
            Object* object = ctx.scene.objects.tryGet(objects[i]);
            if (!object) continue;

            const Transform& start = objectStarts[i];
            object->transform.position = applyLock(center + (start.position - center) * scaling, start.position);
            // A zero scale can't be inverted for lighting, so keep each axis a little away from it
            Vec3 scale = applyLock(start.scale * scaling, start.scale);
            for (f32* axis : { &scale.x, &scale.y, &scale.z }) {
                if (std::abs(*axis) < MIN_OBJECT_SCALE) *axis = *axis < 0.0f ? -MIN_OBJECT_SCALE : MIN_OBJECT_SCALE;
            }
            object->transform.scale = scale;
        }
    }

    // Confirm scale.
    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::ConfirmScale,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext()
        )) {

        ctx.history.commit();

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
                    selection.object
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

        for (u32 i = 0; i < static_cast<u32>(objects.size()) && i < static_cast<u32>(objectStarts.size()); ++i) {
            if (Object* object = ctx.scene.objects.tryGet(objects[i])) object->transform = objectStarts[i];
        }

        ctx.history.cancel(ctx.scene);

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

}