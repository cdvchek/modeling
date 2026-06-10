#include "application/application.hpp"
#include "platform/window/window.hpp"
#include "platform/platform.hpp"

#include <algorithm>
#include <iostream>

bool Application::initialize(AppContext& ctx) {
    ctx.windows.emplace_back(std::make_unique<Window>(1920, 1080));

    if (!ctx.windows[0]->initialize(&ctx.systems.events)) {
        ctx.windows[0]->printWindowError();
        return false;
    }

    ctx.renderer = createRenderer(RendererBackend::RB_OpenGL);

    RendererConfig config{};
    ctx.windows[0]->getDimensions(config.width, config.height);
    config.enableVSync = true;

    if (!ctx.renderer->initialize(ctx.windows[0]->getNativeHandle(), ctx.windows[0]->getNativeDisplayContext(), config)) return false;

    ctx.systems.events.subscribe<Event::KeyDown>(
        [&ctx](const Event::KeyDown& event) -> bool {
            ctx.systems.input.onKey(event.key, true);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::KeyUp>(
        [&ctx](const Event::KeyUp& event) -> bool {
            ctx.systems.input.onKey(event.key, false);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseButtonDown>(
        [&ctx](const Event::MouseButtonDown& event) -> bool {
            ctx.systems.input.onMouseButton(event.button, true);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseButtonUp>(
        [&ctx](const Event::MouseButtonUp& event) -> bool {
            ctx.systems.input.onMouseButton(event.button, false);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseMove>(
        [&ctx](const Event::MouseMove& event) -> bool {
            ctx.systems.input.onMouseMove(event.x, event.y);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::MouseWheel>(
        [&ctx](const Event::MouseWheel& event) -> bool {
            ctx.systems.input.onScroll(event.delta);
            return false;
        }
    );

    ctx.systems.events.subscribe<Event::Quit>(
        [&ctx](const Event::Quit& event) -> bool {
            ctx.is_running = false;
            return true;
        }
    );

    ctx.systems.actions.subscribe(Action::Quit, DefaultKeybinds::Quit);
    ctx.systems.actions.subscribe(Action::ViewportOrbit, DefaultKeybinds::ViewportOrbit);
    ctx.systems.actions.subscribe(Action::ViewportPan, DefaultKeybinds::ViewportPan);
    ctx.systems.actions.subscribe(Action::ViewportZoom, DefaultKeybinds::ViewportZoom);
    ctx.systems.actions.subscribe(Action::Select, DefaultKeybinds::Select);
    ctx.systems.actions.subscribe(Action::Insert, DefaultKeybinds::Insert);
    ctx.systems.actions.subscribe(Action::DeleteSelection, DefaultKeybinds::DeleteSelection);
    ctx.systems.actions.subscribe(Action::Duplicate, DefaultKeybinds::Duplicate);
    ctx.systems.actions.subscribe(Action::Connect, DefaultKeybinds::Connect);
    ctx.systems.actions.subscribe(Action::Disconnect, DefaultKeybinds::Disconnect);
    ctx.systems.actions.subscribe(Action::Save, DefaultKeybinds::Save);
    ctx.systems.actions.subscribe(Action::Undo, DefaultKeybinds::Undo);
    ctx.systems.actions.subscribe(Action::Redo, DefaultKeybinds::Redo);

    ctx.camera.position = Vec3(0.0f, 0.0f, 3.0f);
    ctx.camera.target = Vec3();
    ctx.camera.up = Vec3(0.0f, 1.0f, 0.0f);

    ctx.testObject.meshData.vertices = {
        { Vec3(-0.5f, -0.5f, -0.5f) }, // 0
        { Vec3( 0.5f, -0.5f, -0.5f) }, // 1
        { Vec3( 0.5f,  0.5f, -0.5f) }, // 2
        { Vec3(-0.5f,  0.5f, -0.5f) }, // 3

        { Vec3(-0.5f, -0.5f,  0.5f) }, // 4
        { Vec3( 0.5f, -0.5f,  0.5f) }, // 5
        { Vec3( 0.5f,  0.5f,  0.5f) }, // 6
        { Vec3(-0.5f,  0.5f,  0.5f) }  // 7
    };

    ctx.testObject.meshData.indices = {

        // Front
        4, 5, 6,
        6, 7, 4,

        // Back
        1, 0, 3,
        3, 2, 1,

        // Left
        0, 4, 7,
        7, 3, 0,

        // Right
        5, 1, 2,
        2, 6, 5,

        // Top
        3, 7, 6,
        6, 2, 3,

        // Bottom
        0, 1, 5,
        5, 4, 0
    };

    //ctx.testObject.transform.rotation = Vec3(0.7f, 0.7f, 0.0f);

    ctx.testObject.gpuMesh.create(ctx.testObject.meshData);

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