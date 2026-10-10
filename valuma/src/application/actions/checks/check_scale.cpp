#include "application/actions/checks/action_checks.hpp"
#include "application/tools/tool_guides.hpp"
#include "core/math/vec2.hpp"

#include <algorithm>
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
    const auto& references = ctx.scene.selection.getReferences();
    const auto& referenceStarts = ctx.scene.selection.getReferenceStartTransforms();

    if ((selections.empty() && objects.empty() && references.empty()) || !initTransformTool(ctx)) return;

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
        const ObjectSpace space(ctx.scene.objects.worldTransform(selections[0].object));

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

    // Objects spread out from their shared center (in the world) and grow by the same factor; children come along
    if (!objectStarts.empty()) {
        Vec3 center(0.0f);
        for (const Transform& start : objectStarts) center += start.position;
        center = center / static_cast<f32>(objectStarts.size());

        for (u32 i = 0; i < static_cast<u32>(objects.size()) && i < static_cast<u32>(objectStarts.size()); ++i) {
            Object* object = ctx.scene.objects.tryGet(objects[i]);
            if (!object || carriedByParent(ctx, objects[i])) continue;

            const Transform& start = objectStarts[i];
            ctx.scene.objects.setWorldPosition(objects[i], applyLock(center + (start.position - center) * scaling, start.position));
            // A zero scale can't be inverted for lighting, so keep each axis a little away from it
            Vec3 scale = applyLock(start.scale * scaling, start.scale);
            for (f32* axis : { &scale.x, &scale.y, &scale.z }) {
                if (std::abs(*axis) < MIN_OBJECT_SCALE) *axis = *axis < 0.0f ? -MIN_OBJECT_SCALE : MIN_OBJECT_SCALE;
            }
            // The scale is set relative to the parent's, so the world scale is what was asked for
            const Vec3 parentScale = ctx.scene.objects.parentWorldTransform(objects[i]).scale;
            object->transform.scale = Vec3(scale.x / parentScale.x, scale.y / parentScale.y, scale.z / parentScale.z);
        }
    }

    // Reference images keep their proportions, so they always scale evenly: axis locks don't apply to them
    if (!referenceStarts.empty()) {
        Vec3 center(0.0f);
        for (const Transform& start : referenceStarts) center += start.position;
        center = center / static_cast<f32>(referenceStarts.size());

        for (u32 i = 0; i < static_cast<u32>(references.size()) && i < static_cast<u32>(referenceStarts.size()); ++i) {
            ReferenceImage* image = ctx.scene.references.tryGet(references[i]);
            if (!image) continue;

            image->position = center + (referenceStarts[i].position - center) * scaling;
            image->size = std::max(referenceStarts[i].scale.y * scaling, MIN_REFERENCE_SIZE);
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
            ctx.systems.input_ctx.getModeContext()
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
            if (ctx.scene.objects.isValid(objects[i])) ctx.scene.objects.setWorldTransform(objects[i], objectStarts[i]);
        }

        ctx.history.cancel(ctx.scene);

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getModeContext()
        );
    }

}