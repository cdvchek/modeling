#include "application/application.hpp"

#include "platform/window/window.hpp"
#include "platform/platform.hpp"
#include "application/action_checks/action_checks.hpp"
#include "application/radial_menu.hpp"
#include "application/tool_guides.hpp"
#include "application/status_bar.hpp"
#include "application/light_markers.hpp"
#include "application/main_panel.hpp"
#include "application/console_view.hpp"
#include "application/project_actions.hpp"
#include "core/math/vec4.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace {
    CursorShape toCursorShape(UICursor cursor) {
        switch (cursor) {
            case UICursor::ResizeHorizontal: return CursorShape::ResizeHorizontal;
            case UICursor::ResizeVertical: return CursorShape::ResizeVertical;
            case UICursor::ResizeDiagonalDown: return CursorShape::ResizeDiagonalDown;
            case UICursor::ResizeDiagonalUp: return CursorShape::ResizeDiagonalUp;
            case UICursor::Text: return CursorShape::Text;
            default: return CursorShape::Arrow;
        }
    }

    UIInput makeUIInput(const InputState& input) {
        const u16 buttons[UIInput::BUTTON_COUNT] = {
            static_cast<u16>(MouseButton::Left), static_cast<u16>(MouseButton::Right), static_cast<u16>(MouseButton::Middle)
        };

        UIInput result;
        result.mouse = Vec2(static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()));
        result.mouseDelta = Vec2(static_cast<f32>(input.getMouseDeltaX()), static_cast<f32>(input.getMouseDeltaY()));
        result.scroll = input.getScroll();

        for (u32 i = 0; i < UIInput::BUTTON_COUNT; ++i) {
            result.down[i] = input.isMouseDown(buttons[i]);
            result.pressed[i] = input.wasMousePressedThisFrame(buttons[i]);
            result.released[i] = input.wasMouseReleasedThisFrame(buttons[i]);
        }

        auto held = [&](Key a, Key b) { return input.isKeyDown(static_cast<u16>(a)) || input.isKeyDown(static_cast<u16>(b)); };
        const bool ctrl = held(Key::LeftCtrl, Key::RightCtrl);
        result.shift = held(Key::LeftShift, Key::RightShift);
        result.text = input.getTypedText();

        // Key presses include the OS's repeats, so held keys repeat in text fields too
        for (u16 code : input.getKeyPresses()) {
            switch (static_cast<Key>(code)) {
                case Key::ArrowLeft: result.keys.push_back(UIKey::Left); break;
                case Key::ArrowRight: result.keys.push_back(UIKey::Right); break;
                case Key::Home: result.keys.push_back(UIKey::Home); break;
                case Key::End: result.keys.push_back(UIKey::End); break;
                case Key::Backspace: result.keys.push_back(UIKey::Backspace); break;
                case Key::Delete: result.keys.push_back(UIKey::Delete); break;
                case Key::Enter: result.keys.push_back(UIKey::Enter); break;
                case Key::Escape: result.keys.push_back(UIKey::Escape); break;
                case Key::A: if (ctrl) result.keys.push_back(UIKey::SelectAll); break;
                case Key::C: if (ctrl) result.keys.push_back(UIKey::Copy); break;
                case Key::X: if (ctrl) result.keys.push_back(UIKey::Cut); break;
                case Key::V: if (ctrl) result.keys.push_back(UIKey::Paste); break;
                default: break;
            }
        }

        result.time = std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
        return result;
    }
}

bool Application::initialize(AppContext& ctx) {
    if (!createMainWindow(ctx)) return false;
    if (!setupRenderer(ctx)) return false;

    registerInputEvents(ctx);
    ctx.ui.setClipboard(&Platform::getClipboardText, &Platform::setClipboardText);
    registerDefaultActions(ctx);
    registerCommands(ctx);

    initializeCamera(ctx);
    loadTestScene(ctx);

    ctx.systems.input_ctx.setSelectionContext(InputContext_SelectionVertex);
    initializeProject(ctx);
    ctx.is_running = false;
    return true;
}

void Application::run(AppContext& ctx) {
    ctx.is_running = true;

    while(ctx.is_running) {
        ctx.systems.input.beginFrame();
        Platform::pollEvents();

        // The UI decides first whether it owns the mouse this frame, then the viewport handles the rest
        const ContextManager& contexts = ctx.systems.input_ctx;
        const bool uiInteractive = !ctx.radialMenu.open && !contexts.isActive(InputContext_Console) && contexts.isActive(InputContext_AnySelection);
        ctx.ui.beginFrame(makeUIInput(ctx.systems.input), uiInteractive);
        ctx.systems.actions.setMouseBlocked(ctx.ui.wantsMouse());
        ctx.systems.actions.setKeyboardBlocked(ctx.ui.wantsKeyboard());

        checkActions(ctx);
        updateWindowTitle(ctx);
        Application::renderFrame(ctx);

        ctx.windows[0]->setCursor(toCursorShape(ctx.ui.cursor()));
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
            if (!light.enabled) continue;

            if (light.type == LightType::Directional) {
                if (state.directionalCount == MAX_DIRECTIONAL_LIGHTS) continue;

                state.directionalDirections[state.directionalCount] = light.direction.normalized();
                state.directionalColors[state.directionalCount] = light.color * light.intensity;
                ++state.directionalCount;
                continue;
            }

            if (state.localCount == MAX_LOCAL_LIGHTS) continue;

            const u32 i = state.localCount++;
            state.localPositions[i] = light.position;
            state.localColors[i] = light.color * light.intensity;
            state.localRanges[i] = light.range;

            if (light.type == LightType::Spot) {
                state.localDirections[i] = light.direction.normalized();
                state.localCosInner[i] = std::cos(light.innerConeRadians);
                // smoothstep needs outer < inner
                state.localCosOuter[i] = std::min(std::cos(light.outerConeRadians), state.localCosInner[i] - 1e-4f);
            } else {
                state.localDirections[i] = Vec3(0.0f, -1.0f, 0.0f);
                state.localCosInner[i] = -1.0f;
                state.localCosOuter[i] = -2.0f;
            }
        }

        return state;
    }
}

void Application::renderFrame(AppContext& ctx) {
    u32 width = 0;
    u32 height = 0;
    ctx.windows[0]->getDimensions(width, height);

    if (width == 0 || height == 0) return;

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    ctx.frameTimer.tick(std::chrono::duration<f64>(now).count());

    ctx.renderer->beginFrame();
    ctx.renderer->beginMainPass(ctx.renderer->m_clearState);

    f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);

    Mat4 view = ctx.scene.camera.getViewMatrix();
    Mat4 projection = ctx.scene.camera.getProjectionMatrix(aspectRatio);
    Mat4 viewProjection = projection * view;

    ctx.renderer->setLighting(buildLightingState(ctx.scene.lights, ctx.viewport.headlight, ctx.scene.camera));

    ctx.objectMeshes.prune(ctx.scene.objects);

    for (ObjectHandle handle : ctx.scene.objects.handles()) {
        Object& object = ctx.scene.objects.get(handle);

        Mat4 model = object.transform.getMatrix();
        Mat4 mvp = viewProjection * model;

        const Selection& selection = ctx.scene.selection;
        
        DrawCommand cmd;

        // Only the object being edited shows its wireframe and selection; the others show just their faces
        const bool active = handle == selection.getActiveObject();
        const u32 selectionContext = ctx.systems.input_ctx.getSelectionContext();
        const bool objectMode = selectionContext == InputContext_SelectionObject;

        cmd.showVerts = active && (selectionContext & InputContext_SelectionVertex);
        cmd.showEdges = objectMode ? selection.hasObject(handle) : active;
        cmd.showFaces = true;

        // In object mode a selected object is outlined: every edge in the selection color
        if (objectMode) {
            if (selection.hasObject(handle)) cmd.highlightedEdges = object.meshData.getEdgeHandles();
        } else if (active) {
            cmd.highlightedVerts = selection.getVertexHandles();
            cmd.highlightedEdges = selection.getEdgeHandles();
            cmd.highlightedFaces = selection.getFaceHandles();

            // Selected faces are outlined with the same treatment as selected edges
            for (FaceHandle face : cmd.highlightedFaces) {
                const Face* data = object.meshData.getFace(face);
                if (!data) continue;
                for (EdgeHandle edge : object.meshData.getLoopEdges(data->edge)) cmd.highlightedEdges.push_back(edge);
            }
        }
        
        cmd.mesh = ctx.objectMeshes.sync(handle, object);
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

    UIDrawList& ui = ctx.uiDrawList;
    ui.clear();

    drawLightMarkers(ctx, ui, viewProjection, static_cast<f32>(width), static_cast<f32>(height));
    drawToolGuides(ctx, ui);
    drawStatusBar(ctx, ui, static_cast<f32>(width), static_cast<f32>(height));

    ctx.ui.setDrawList(&ui);
    ctx.ui.setFont(makeUIFont(FontId::UI, ctx.fonts.get(FontId::UI)));
    ctx.ui.beginDraw();
    if (ctx.viewport.showPanel) drawMainPanel(ctx, { 0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height) - statusBarHeight(ctx) });
    ctx.ui.endDraw();

    drawRadialMenu(ctx, ui);

    if (ctx.systems.input_ctx.isActive(InputContext_Console)) {
        drawConsole(ctx, ui, { 0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height) - statusBarHeight(ctx) });
    }

    ctx.renderer->drawUI(ui);

    ctx.renderer->endMainPass();
    ctx.renderer->endFrame();
    ctx.renderer->present();
}