#include "application/action_checks/action_checks.hpp"
#include "scene/selection/ray.hpp"
#include "scene/selection/scene_queries.hpp"

#include <algorithm>

void checkSelectionContext(AppContext& ctx) {
    ActionMap& actions = ctx.systems.actions;
    InputState& input = ctx.systems.input;
    ContextManager& ictx = ctx.systems.input_ctx;
    Camera& camera = ctx.scene.camera;

    if (actions.isActionDown(Action::ViewportOrbit, input, ictx.getContext())) {
        i32 dx = input.getMouseDeltaX();
        i32 dy = input.getMouseDeltaY();

        dx = std::clamp(dx, -500, 500);
        dy = std::clamp(dy, -500, 500);

        camera.yaw -= (f32)dx * 0.005f;
        camera.pitch += (f32)dy * 0.005f;

        camera.pitch = std::clamp(camera.pitch, -1.552f, 1.552f);
        camera.updatePositionFromOrbit();
    }

    if (actions.isActionDown(Action::ViewportPan, input, ictx.getContext())) {
        Vec3 right = camera.getRight();
        Vec3 cameraUp = Vec3::cross(right, camera.getForward()).normalized();

        f32 panSpeed = 0.001f * camera.distance;

        Vec3 pan = (-right * input.getMouseDeltaX() + cameraUp * input.getMouseDeltaY()) * panSpeed;
        
        camera.position += pan;
        camera.target += pan;
    }

    i32 zoom = 0;
    if (actions.isActionDown(Action::ViewportZoom, input, ictx.getContext(), &zoom)) {
        camera.distance -= zoom * 0.005f;
        if (camera.distance <= 0.5) camera.distance = 0.5;
        camera.updatePositionFromOrbit();
    }

    if (actions.wasActionPressedThisFrame(Action::Select, input, ictx.getContext())) {
        u32 width = 0;
        u32 height = 0;

        ctx.windows[0]->getDimensions(width, height);

        Ray ray = makeRayFromScreenPosition(
            input.getMouseX(),
            input.getMouseY(),
            width,
            height,
            camera
        );

        VertexHit hit = pickVertex(
            ctx.scene,
            ray,
            0.03f
        );

        bool addDown = actions.isActionDown(Action::AddSelection, input, ictx.getContext());
        bool removeDown = actions.isActionDown(Action::RemoveSelection, input, ictx.getContext());

        if (!addDown && !removeDown) {
            ctx.scene.selection.clear();
        }

        if (hit.hit) {
            if (removeDown) {
                ctx.scene.selection.removeVertex(
                    hit.objectIndex,
                    hit.vertexIndex
                );
            } else {
                ctx.scene.selection.addVertex(
                    hit.objectIndex,
                    hit.vertexIndex,
                    ctx.scene.objects.get(hit.objectIndex).meshData.getVertices()[hit.vertexIndex].position
                );
            }
        }
    }

    if (ctx.scene.selection.hasVertices() && actions.wasActionPressedThisFrame(Action::GrabSelection, input, ictx.getContext())) {
        ictx.setContext(InputContext_Grab);
    }
}