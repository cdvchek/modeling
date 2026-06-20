#include "application/application.hpp"
#include "platform/window/window.hpp"
#include "platform/platform.hpp"
#include "scene/selection/ray.hpp"
#include "scene/selection/scene_queries.hpp"
#include "core/math/vec4.hpp"

#include <algorithm>
#include <iostream>

bool Application::initialize(AppContext& ctx) {
    if (!createMainWindow(ctx)) return false;
    if (!setupRenderer(ctx)) return false;

    registerInputEvents(ctx);
    registerDefaultActions(ctx);

    initializeCamera(ctx);
    loadTestScene(ctx);

    ctx.is_running = false;
    return true;
}

void Application::run(AppContext& ctx) {
    ctx.is_running = true;

    while(ctx.is_running) {
        ctx.systems.input.beginFrame();
        
        Platform::pollEvents();
        
        if (ctx.systems.actions.isActionDown(Action::Quit, ctx.systems.input)) {
            ctx.systems.events.trigger(Event::Quit{});
            break;
        }

        if (ctx.systems.actions.isActionDown(Action::ViewportOrbit, ctx.systems.input)) {
            i32 dx = ctx.systems.input.getMouseDeltaX();
            i32 dy = ctx.systems.input.getMouseDeltaY();

            if (std::abs(dx) < 500 && std::abs(dy) < 500) {
                ctx.scene.camera.yaw -= dx * 0.005f;
                ctx.scene.camera.pitch += dy * 0.005f;
            }

            ctx.scene.camera.pitch = std::clamp(ctx.scene.camera.pitch, -1.5f, 1.5f);
            ctx.scene.camera.updatePositionFromOrbit();
        }

        if (ctx.systems.actions.isActionDown(Action::ViewportPan, ctx.systems.input)) {
            Vec3 right = ctx.scene.camera.getRight();
            Vec3 cameraUp = Vec3::cross(right, ctx.scene.camera.getForward()).normalized();

            f32 panSpeed = 0.001f * ctx.scene.camera.distance;

            Vec3 pan = (-right * ctx.systems.input.getMouseDeltaX() + cameraUp * ctx.systems.input.getMouseDeltaY()) * panSpeed;
            
            ctx.scene.camera.position += pan;
            ctx.scene.camera.target += pan;
        }

        i32 zoom = 0;
        if (ctx.systems.actions.isActionDown(Action::ViewportZoom, ctx.systems.input, &zoom)) {
            ctx.scene.camera.distance -= zoom * 0.005f;
            if (ctx.scene.camera.distance <= 0.5) ctx.scene.camera.distance = 0.5;
            ctx.scene.camera.updatePositionFromOrbit();
        }

        if (ctx.systems.actions.wasActionPressedThisFrame(Action::Select, ctx.systems.input)) {
            u32 width = 0;
            u32 height = 0;

            ctx.windows[0]->getDimensions(width, height);

            Ray ray = makeRayFromScreenPosition(
                ctx.systems.input.getMouseX(),
                ctx.systems.input.getMouseY(),
                width,
                height,
                ctx.scene.camera
            );

            VertexHit hit = pickVertex(
                ctx.scene,
                ray,
                0.03f
            );

            bool addDown = ctx.systems.actions.isActionDown(Action::AddSelection, ctx.systems.input);
            bool removeDown = ctx.systems.actions.isActionDown(Action::RemoveSelection, ctx.systems.input);

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
                        hit.vertexIndex
                    );
                }
            }
        }

        Application::renderFrame(ctx);
    }
} 

void Application::renderFrame(AppContext& ctx) {
    ctx.renderer->beginFrame();
    ctx.renderer->beginMainPass(ctx.renderer->m_clearState);

    u32 width = 0;
    u32 height = 0;
    ctx.windows[0]->getDimensions(width, height);

    f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);

    Mat4 view = ctx.scene.camera.getViewMatrix();
    Mat4 projection = ctx.scene.camera.getProjectionMatrix(aspectRatio);

    for (u32 i = 0; i < ctx.scene.objects.count(); i++) {
        Object& object = ctx.scene.objects.get(i);

        Mat4 model = object.transform.getMatrix();
        Mat4 mvp = projection * view * model;

        DrawCommand cmd;
        cmd.mesh = &object.gpuMesh;
        cmd.mvp = mvp;

        ctx.renderer->draw(cmd);
    }

    Mat4 viewProjection = projection * view;

    for (const VertexSelection& selection : ctx.scene.selection.getVertices()) {
        const Object& object = ctx.scene.objects.get(selection.objectIndex);
        const Vertex& vertex = object.meshData.vertices[selection.vertexIndex];

        Vec4 worldPos4 = object.transform.getMatrix() * Vec4(
            vertex.position.x,
            vertex.position.y,
            vertex.position.z,
            1.0f
        );

        PointDrawCommand pointCmd;
        pointCmd.position = Vec3(worldPos4.x, worldPos4.y, worldPos4.z);
        pointCmd.viewProjection = viewProjection;
        pointCmd.size = 14.0f;
        pointCmd.color = Vec3(1.0f, 0.8f, 0.0f);

        ctx.renderer->drawPoint(pointCmd);
    }

    ctx.renderer->endMainPass();
    ctx.renderer->endFrame();
    ctx.renderer->present();
}