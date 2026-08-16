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

    ctx.systems.input_ctx.setSelectionContext(InputContext_SelectionVertex);
    ctx.is_running = false;
    return true;
}

void Application::run(AppContext& ctx) {
    ctx.is_running = true;

    while(ctx.is_running) {
        ctx.systems.input.beginFrame();
        Platform::pollEvents();
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
    Mat4 viewProjection = projection * view;

    for (u32 i = 0; i < ctx.scene.objects.count(); i++) {
        Object& object = ctx.scene.objects.get(i);

        if (object.meshDirty) {
            object.gpuMesh.update(object.meshData);
            object.meshDirty = false;
        }

        Mat4 model = object.transform.getMatrix();
        Mat4 mvp = viewProjection * model;

        DrawCommand cmd(
            &object.gpuMesh,
            ctx.scene.selection.getVertexIndices(),
            mvp
        );

        ctx.renderer->draw(cmd);
    }

    ctx.renderer->endMainPass();
    ctx.renderer->endFrame();
    ctx.renderer->present();
}