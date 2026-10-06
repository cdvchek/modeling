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
    const auto& lights = ctx.scene.selection.getLights();
    const auto& lightStarts = ctx.scene.selection.getLightStartPositions();
    const auto& objects = ctx.scene.selection.getObjects();
    const auto& objectStarts = ctx.scene.selection.getObjectStartTransforms();

    if (selections.empty() && lights.empty() && objects.empty()) return;

    // Vertices live in their object's mesh space; the grab works in world space and converts
    const ObjectSpace space(selections.empty() ? Transform() : ctx.scene.objects.get(selections[0].object).transform);

    const auto& starts =
        ctx.scene.selection.getSelectionStartPositions();

    // Check if an axis was just activated.
    const bool xJustActivated = xAxis && !wasXAxis;
    const bool yJustActivated = yAxis && !wasYAxis;
    const bool zJustActivated = zAxis && !wasZAxis;

    // Snap vertices back onto the selected axis.
    if (xJustActivated || yJustActivated || zJustActivated) {
        // Lights snap the same way as vertices
        for (u32 i = 0; i < static_cast<u32>(lights.size()) && i < static_cast<u32>(lightStarts.size()); ++i) {
            Light* light = ctx.scene.lights.tryGet(lights[i]);
            if (!light) continue;

            const Vec3& start = lightStarts[i];
            if (xJustActivated) { light->position.y = start.y; light->position.z = start.z; }
            else if (yJustActivated) { light->position.x = start.x; light->position.z = start.z; }
            else if (zJustActivated) { light->position.x = start.x; light->position.y = start.y; }
        }

        // So do whole objects
        for (u32 i = 0; i < static_cast<u32>(objects.size()) && i < static_cast<u32>(objectStarts.size()); ++i) {
            Object* object = ctx.scene.objects.tryGet(objects[i]);
            if (!object) continue;

            Vec3& position = object->transform.position;
            const Vec3& start = objectStarts[i].position;
            if (xJustActivated) { position.y = start.y; position.z = start.z; }
            else if (yJustActivated) { position.x = start.x; position.z = start.z; }
            else if (zJustActivated) { position.x = start.x; position.y = start.y; }
        }

        for (u32 i = 0; i < static_cast<u32>(selections.size()); ++i) {
            const VertexSelection& selection = selections[i];
            const Vec3 start = space.pointToWorld(starts[i]);

            Object& obj =
                ctx.scene.objects.get(selection.object);

            Vec3 currPos =
                space.pointToWorld(obj.meshData.getVertexPosition(selection.vertex));

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
                space.pointToLocal(newPos)
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

    // Move selected lights and objects.
    for (LightHandle handle : lights) {
        if (Light* light = ctx.scene.lights.tryGet(handle)) light->position += vertMove;
    }
    for (ObjectHandle handle : objects) {
        if (Object* object = ctx.scene.objects.tryGet(handle)) object->transform.position += vertMove;
    }

    // Move selected vertices by the same world movement, in their mesh space.
    const Vec3 localMove = space.directionToLocal(vertMove);

    for (const VertexSelection& vs : selections) {
        Object& obj =
            ctx.scene.objects.get(vs.object);

        obj.meshData.translateVertex(
            vs.vertex,
            localMove
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
                ctx.scene.objects.get(selection.object);

            obj.meshData.positionVertex(
                selection.vertex,
                start
            );

            obj.meshData.setFacesDirtyByVertex(
                selection.vertex
            );

            obj.meshDirty = true;
        }

        for (u32 i = 0; i < static_cast<u32>(lights.size()) && i < static_cast<u32>(lightStarts.size()); ++i) {
            if (Light* light = ctx.scene.lights.tryGet(lights[i])) light->position = lightStarts[i];
        }

        for (u32 i = 0; i < static_cast<u32>(objects.size()) && i < static_cast<u32>(objectStarts.size()); ++i) {
            if (Object* object = ctx.scene.objects.tryGet(objects[i])) object->transform = objectStarts[i];
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