#include "application/application.hpp"
#include "platform/window/window.hpp"
#include "platform/platform.hpp"
#include "application/action_checks/action_checks.hpp"
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

    ctx.systems.input_ctx.addContext(InputContext_Selection);
    ctx.is_running = false;
    return true;
}

void Application::run(AppContext& ctx) {
    ctx.is_running = true;

    while(ctx.is_running) {
        ctx.systems.input.beginFrame();
        
        Platform::pollEvents();
        
        if (ctx.systems.actions.isActionDown(Action::Quit, ctx.systems.input, ctx.systems.input_ctx.getContext())) {
            ctx.systems.events.trigger(Event::Quit{});
            break;
        }

        checkActions(ctx);

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

        if (object.meshDirty) {
            object.gpuMesh.update(object.meshData);
            object.meshDirty = false;
        }

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