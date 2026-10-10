#include "application/actions/checks/action_checks.hpp"
#include "application/tools/tool_guides.hpp"
#include "application/actions/origin_actions.hpp"
#include "core/math/screen_drag.hpp"

#include <cmath>

namespace {
    // Rodrigues' rotation formula
    Vec3 rotateAround(Vec3 v, Vec3 axis, f32 cosAngle, f32 sinAngle) {
        return v * cosAngle + Vec3::cross(axis, v) * sinAngle + axis * Vec3::dot(axis, v) * (1.0f - cosAngle);
    }
}

void checkRotateContext(AppContext& ctx) {
    const bool xAxis = ctx.systems.input_ctx.isActive(InputContext_XAxis);
    const bool yAxis = ctx.systems.input_ctx.isActive(InputContext_YAxis);
    const bool zAxis = ctx.systems.input_ctx.isActive(InputContext_ZAxis);

    const auto& selections = ctx.scene.selection.getVertices();
    const auto& lights = ctx.scene.selection.getLights();
    const auto& lightStarts = ctx.scene.selection.getLightStartDirections();
    const auto& starts = ctx.scene.selection.getSelectionStartPositions();
    const auto& objects = ctx.scene.selection.getObjects();
    const auto& objectStarts = ctx.scene.selection.getObjectStartTransforms();
    const auto& references = ctx.scene.selection.getReferences();
    const auto& referenceStarts = ctx.scene.selection.getReferenceStartTransforms();

    const bool origin = ctx.scene.selection.hasOrigin() && ctx.scene.objects.isValid(ctx.originEdit.object);
    if (selections.empty() && lights.empty() && objects.empty() && !origin && references.empty()) return;

    // Puts every selected light's direction back to where it was when the rotation started
    const auto restoreLightDirections = [&]() {
        for (u32 i = 0; i < static_cast<u32>(lights.size()) && i < static_cast<u32>(lightStarts.size()); ++i) {
            if (Light* light = ctx.scene.lights.tryGet(lights[i])) light->direction = lightStarts[i];
        }
    };

    if (!initTransformTool(ctx)) return;

    // Add up how far the mouse has swept around the pivot, so circling more than once keeps turning
    TransformTool& tool = ctx.transformTool;
    const Vec2 mouse(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));
    const f32 mouseAngle = screenAngle(tool.pivot, mouse);
    tool.angle += wrapAngle(mouseAngle - tool.lastAngle);
    tool.lastAngle = mouseAngle;

    // Vertices turn in world space around their world center, then go back to mesh space
    const ObjectSpace space(selections.empty() ? Transform() : ctx.scene.objects.worldTransform(selections[0].object));

    Vec3 center(0.0f);
    for (const Vec3& start : starts) center += space.pointToWorld(start);
    if (!starts.empty()) center = center / static_cast<f32>(starts.size());

    // View axis by default, or the locked world axis
    const Vec3 forward = ctx.scene.camera.getForward().normalized();
    Vec3 axis = forward;
    if (xAxis) axis = Vec3(1.0f, 0.0f, 0.0f);
    else if (yAxis) axis = Vec3(0.0f, 1.0f, 0.0f);
    else if (zAxis) axis = Vec3(0.0f, 0.0f, 1.0f);

    // Counterclockwise on screen is counterclockwise around an axis pointing at the viewer
    if (Vec3::dot(axis, forward) > 0.0f) axis = -axis;

    const f32 cosAngle = std::cos(tool.angle);
    const f32 sinAngle = std::sin(tool.angle);

    // Lights turn in place: only their direction rotates
    for (u32 i = 0; i < static_cast<u32>(lights.size()) && i < static_cast<u32>(lightStarts.size()); ++i) {
        if (Light* light = ctx.scene.lights.tryGet(lights[i])) light->direction = rotateAround(lightStarts[i], axis, cosAngle, sinAngle).normalized();
    }

    for (u32 i = 0; i < static_cast<u32>(selections.size()); ++i) {
        const VertexSelection& selection = selections[i];
        Object& object = ctx.scene.objects.get(selection.object);

        object.meshData.positionVertex(selection.vertex, space.pointToLocal(center + rotateAround(space.pointToWorld(starts[i]) - center, axis, cosAngle, sinAngle)));
        object.meshData.setFacesDirtyByVertex(selection.vertex);
        object.meshDirty = true;
    }

    // An origin turns in place: the object's axes rotate while its mesh stays put
    if (origin) {
        Transform to = ctx.originEdit.start.world;
        to.rotation = rotateEuler(to.rotation, axis, tool.angle);
        updateOriginEdit(ctx, to);
    }

    // Objects orbit their shared center and turn by the same amount
    if (!objectStarts.empty()) {
        Vec3 objectCenter(0.0f);
        for (const Transform& start : objectStarts) objectCenter += start.position;
        objectCenter = objectCenter / static_cast<f32>(objectStarts.size());

        for (u32 i = 0; i < static_cast<u32>(objects.size()) && i < static_cast<u32>(objectStarts.size()); ++i) {
            if (!ctx.scene.objects.isValid(objects[i]) || carriedByParent(ctx, objects[i])) continue;

            // Rebuilt from the start each frame in the world, then made relative to the parent
            Transform world = objectStarts[i];
            world.position = objectCenter + rotateAround(world.position - objectCenter, axis, cosAngle, sinAngle);
            world.rotation = rotateEuler(world.rotation, axis, tool.angle);
            ctx.scene.objects.setWorldTransform(objects[i], world);
        }
    }

    // Reference images orbit their shared center and turn, like objects
    if (!referenceStarts.empty()) {
        Vec3 referenceCenter(0.0f);
        for (const Transform& start : referenceStarts) referenceCenter += start.position;
        referenceCenter = referenceCenter / static_cast<f32>(referenceStarts.size());

        for (u32 i = 0; i < static_cast<u32>(references.size()) && i < static_cast<u32>(referenceStarts.size()); ++i) {
            ReferenceImage* image = ctx.scene.references.tryGet(references[i]);
            if (!image) continue;

            image->position = referenceCenter + rotateAround(referenceStarts[i].position - referenceCenter, axis, cosAngle, sinAngle);
            image->rotation = rotateEuler(referenceStarts[i].rotation, axis, tool.angle);
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
            ctx.systems.input_ctx.getModeContext()
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

        restoreLightDirections();

        for (u32 i = 0; i < static_cast<u32>(objects.size()) && i < static_cast<u32>(objectStarts.size()); ++i) {
            if (ctx.scene.objects.isValid(objects[i])) ctx.scene.objects.setWorldTransform(objects[i], objectStarts[i]);
        }

        ctx.history.cancel(ctx.scene);

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getModeContext()
        );
    }

}