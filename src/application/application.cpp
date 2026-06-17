#include "application/application.hpp"
#include "platform/window/window.hpp"
#include "platform/platform.hpp"

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
            ctx.camera.yaw -= ctx.systems.input.getMouseDeltaX() * 0.005f;
            ctx.camera.pitch += ctx.systems.input.getMouseDeltaY() * 0.005f;

            ctx.camera.pitch = std::clamp(ctx.camera.pitch, -1.5f, 1.5f);

            ctx.camera.updatePositionFromOrbit();
        }

        if (ctx.systems.actions.isActionDown(Action::ViewportPan, ctx.systems.input)) {
            Vec3 right = ctx.camera.getRight();
            Vec3 cameraUp = Vec3::cross(right, ctx.camera.getForward()).normalized();

            f32 panSpeed = 0.001f * ctx.camera.distance;

            Vec3 pan = (-right * ctx.systems.input.getMouseDeltaX() + cameraUp * ctx.systems.input.getMouseDeltaY()) * panSpeed;
            
            ctx.camera.position += pan;
            ctx.camera.target += pan;
        }

        i32 zoom = 0;
        if (ctx.systems.actions.isActionDown(Action::ViewportZoom, ctx.systems.input, &zoom)) {
            ctx.camera.distance -= zoom * 0.005f;
            if (ctx.camera.distance <= 0.5) ctx.camera.distance = 0.5;
            ctx.camera.updatePositionFromOrbit();
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

    Mat4 model = ctx.testObject.transform.getMatrix();
    Mat4 view = ctx.camera.getViewMatrix();
    Mat4 projection = ctx.camera.getProjectionMatrix(aspectRatio);

    Mat4 mvp = projection * view * model;

    DrawCommand cmd;
    cmd.mesh = &ctx.testObject.gpuMesh;
    cmd.mvp = mvp;

    ctx.renderer->draw(cmd);

    // for (const DrawCommand& cmd : ctx.rendererDrawCommands) {
    //     ctx.renderer->draw(cmd);
    // }

    ctx.renderer->endMainPass();
    ctx.renderer->endFrame();
    ctx.renderer->present();
}