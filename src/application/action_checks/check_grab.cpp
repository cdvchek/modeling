#include "application/action_checks/action_checks.hpp"

#include <algorithm>

void checkGrabContext(AppContext& ctx) {
    for (VertexSelection vs : ctx.scene.selection.getVertices()) {
        Object& obj = ctx.scene.objects.all()[vs.objectIndex];
        Vertex& vert = obj.meshData.vertices[vs.vertexIndex];

        i32 dx = ctx.systems.input.getMouseDeltaX();
        i32 dy = ctx.systems.input.getMouseDeltaY();

        dx = std::clamp(dx, -500, 500);
        dy = std::clamp(dy, -500, 500);

        Vec3 right = ctx.scene.camera.getRight();
        Vec3 cameraUp = Vec3::cross(right, ctx.scene.camera.getForward()).normalized();

        f32 vertMoveSpeed = 0.001f * ctx.scene.camera.distance;

        Vec3 vertMove = (right * dx - cameraUp * dy) * vertMoveSpeed;

        vert.position += vertMove;
        obj.meshDirty = true;
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(Action::ConfirmGrab, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
        ctx.systems.input_ctx.setContext(InputContext_Selection);
    }
}