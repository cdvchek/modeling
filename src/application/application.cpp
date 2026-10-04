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
    registerCommands(ctx);

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

namespace {
    LightingState buildLightingState(const LightCollection& lights, const Headlight& headlight, const Camera& camera) {
        LightingState state;

        const AmbientLight& ambient = lights.getAmbient();
        state.ambientColor = ambient.color;
        state.ambientStrength = ambient.strength;

        // Headlight goes first so scene lights can never push it out
        if (headlight.enabled) {
            state.directionalDirections[0] = camera.getForward();
            state.directionalColors[0] = headlight.color * headlight.strength;
            state.directionalCount = 1;
        }

        for (LightHandle handle : lights.handles()) {
            const Light& light = lights.get(handle);
            if (!light.enabled || light.type != LightType::Directional) continue;
            if (state.directionalCount == MAX_DIRECTIONAL_LIGHTS) break;

            state.directionalDirections[state.directionalCount] = light.direction.normalized();
            state.directionalColors[state.directionalCount] = light.color * light.intensity;
            ++state.directionalCount;
        }

        return state;
    }
}

void Application::renderFrame(AppContext& ctx) {
    u32 width = 0;
    u32 height = 0;
    ctx.windows[0]->getDimensions(width, height);

    if (width == 0 || height == 0) return;

    ctx.renderer->beginFrame();
    ctx.renderer->beginMainPass(ctx.renderer->m_clearState);

    f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);

    Mat4 view = ctx.scene.camera.getViewMatrix();
    Mat4 projection = ctx.scene.camera.getProjectionMatrix(aspectRatio);
    Mat4 viewProjection = projection * view;

    ctx.renderer->setLighting(buildLightingState(ctx.scene.lights, ctx.viewport.headlight, ctx.scene.camera));

    for (u32 i = 0; i < ctx.scene.objects.count(); i++) {
        Object& object = ctx.scene.objects.get(i);

        if (object.meshDirty) {
            object.gpuMesh.update(object.meshData);
            object.meshDirty = false;
        }

        Mat4 model = object.transform.getMatrix();
        Mat4 mvp = viewProjection * model;

        const Selection& selection = ctx.scene.selection;
        
        DrawCommand cmd;

        u32 selectionContext = ctx.systems.input_ctx.getSelectionContext();

        cmd.showVerts = selectionContext & InputContext_SelectionVertex;
        cmd.showEdges = true;
        cmd.showFaces = true;

        cmd.highlightedVerts = selection.getVertexHandles();
        cmd.highlightedEdges = selection.getEdgeHandles();
        cmd.highlightedFaces = selection.getFaceHandles();
        
        cmd.mesh = &object.gpuMesh;
        cmd.model = model;
        cmd.mvp = mvp;

        ctx.renderer->draw(cmd);
    }

    DrawGridCommand gridCmd;
    gridCmd.viewProjection = viewProjection;
    gridCmd.cameraPosition = ctx.scene.camera.position;
    gridCmd.cameraDistance = ctx.scene.camera.distance;
    gridCmd.farPlane = ctx.scene.camera.farPlane;

    ctx.renderer->drawGrid(gridCmd);

    if (ctx.systems.input_ctx.isActive(InputContext_Debug)) {
        ctx.debug_renderer.render(
            *ctx.renderer,
            ctx.scene,
            viewProjection
        );
    }

    if (ctx.systems.input_ctx.isActive(InputContext_Console)) {
        ctx.renderer->drawConsoleBackground();
        
        u32 width;
        u32 height;
        ctx.windows[0].get()->getDimensions(width, height);

        const std::vector<std::string>& commandHistory = ctx.systems.console.getHistory();
        u32 j = 1;
        for (u32 i = static_cast<u32>(commandHistory.size()); i-- > 0;) {
            const std::string& command = commandHistory[i];

            ctx.renderer->drawText(
                DrawTextCommand{
                    command,
                    25.0f,
                    static_cast<f32>(height) - (84.0f + (28.0f * j++))
                }
            );
        }

        ctx.renderer->drawText(
            DrawTextCommand{
                ctx.systems.console.getCurrentCommand(),
                25.0f,
                static_cast<f32>(height) - 49
            }
        );
    }

    ctx.renderer->endMainPass();
    ctx.renderer->endFrame();
    ctx.renderer->present();
}