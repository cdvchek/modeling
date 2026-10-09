#include "application/application.hpp"
#include "application/ui/top_bar.hpp"

#include "platform/window/window.hpp"
#include "platform/platform.hpp"
#include "application/actions/checks/action_checks.hpp"
#include "application/ui/radial_menu.hpp"
#include "application/tools/tool_guides.hpp"
#include "application/ui/status_bar.hpp"
#include "application/viewport/light_markers.hpp"
#include "application/viewport/origin_markers.hpp"
#include "application/ui/main_panel.hpp"
#include "application/ui/console_view.hpp"
#include "application/actions/project_actions.hpp"
#include "application/ui/modal_windows.hpp"
#include "application/viewport/reference_images.hpp"
#include "application/viewport/material_view.hpp"
#include "application/ui/stats_overlay.hpp"
#include "core/math/vec4.hpp"
#include "core/math/color.hpp"

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

    // Each workspace has its own tools: actions and console commands that don't belong to it are switched off
    ctx.systems.actions.setFilter([&ctx](Action action) { return actionAllowed(ctx.workspace.current, action); });
    ctx.systems.commands.setGuard([&ctx](const std::string& name) {
        if (ctx.workspace.current == Workspace::Model || !isModelCommand(name)) return true;
        ctx.systems.console.printError(name + ": a modeling command; switch to the Model workspace to use it");
        return false;
    });

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

        const auto inputStart = std::chrono::steady_clock::now();
        checkActions(ctx);
        ctx.frameStats.inputMilliseconds = std::chrono::duration<f32, std::milli>(std::chrono::steady_clock::now() - inputStart).count();
        updateWindowTitle(ctx);
        Application::renderFrame(ctx);

        ctx.windows[0]->setCursor(toCursorShape(ctx.ui.cursor()));
    }
}

namespace {
    // The environment's sky and ground: brighter above, darker below, averaging out to the ambient light
    constexpr f32 SKY_SCALE = 1.4f;
    constexpr f32 GROUND_SCALE = 0.6f;
    // How much of the background's hue they take (1 all of it); half keeps gray surfaces from turning blue
    constexpr f32 BACKGROUND_TINT = 0.5f;

    // A color's hue at brightness 1, faded toward white by amount
    Vec3 tintOf(const Vec3& srgb, f32 amount) {
        const Vec3 linear = srgbToLinear(srgb);
        const f32 luminance = 0.2126f * linear.x + 0.7152f * linear.y + 0.0722f * linear.z;
        if (luminance <= 0.0f) return Vec3(1.0f);
        const Vec3 hue = linear / luminance;
        return Vec3(1.0f + (hue.x - 1.0f) * amount, 1.0f + (hue.y - 1.0f) * amount, 1.0f + (hue.z - 1.0f) * amount);
    }

    Vec3 multiply(const Vec3& a, const Vec3& b) {
        return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
    }

    // Colors are picked in sRGB; the shader lights in linear, so they're converted here
    LightingState buildLightingState(const LightCollection& lights, const Headlight& headlight, const Camera& camera, const BackgroundGradient& background) {
        LightingState state;

        const AmbientLight& ambient = lights.getAmbient();
        state.ambientColor = srgbToLinear(ambient.color);
        state.ambientStrength = ambient.strength;

        const Vec3 ambientLight = state.ambientColor * ambient.strength;
        state.skyColor = multiply(ambientLight, tintOf(background.top, BACKGROUND_TINT)) * SKY_SCALE;
        state.groundColor = multiply(ambientLight, tintOf(background.bottom, BACKGROUND_TINT)) * GROUND_SCALE;
        state.cameraPosition = camera.position;

        // Headlight goes first so scene lights can never push it out
        if (headlight.enabled) {
            state.directionalDirections[0] = camera.getForward();
            state.directionalColors[0] = srgbToLinear(headlight.color) * headlight.strength;
            state.directionalCount = 1;
        }

        for (LightHandle handle : lights.handles()) {
            const Light& light = lights.get(handle);
            if (!light.enabled) continue;

            if (light.type == LightType::Directional) {
                if (state.directionalCount == MAX_DIRECTIONAL_LIGHTS) continue;

                state.directionalDirections[state.directionalCount] = light.direction.normalized();
                state.directionalColors[state.directionalCount] = srgbToLinear(light.color) * light.intensity;
                ++state.directionalCount;
                continue;
            }

            if (state.localCount == MAX_LOCAL_LIGHTS) continue;

            const u32 i = state.localCount++;
            state.localPositions[i] = light.position;
            state.localColors[i] = srgbToLinear(light.color) * light.intensity;
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

    const auto start = std::chrono::steady_clock::now();
    ctx.frameTimer.tick(std::chrono::duration<f64>(start.time_since_epoch()).count());

    ctx.renderer->beginFrame();
    // Textures first (swatches and objects draw with them); swatches render into their own targets, so before the main pass
    syncTextures(ctx);
    ctx.materialPreviews.sync(ctx);
    ctx.renderer->beginMainPass(ctx.renderer->m_clearState);

    // The 3D scene draws only in its part of the window (all of it below the top bar, or the left side in UV)
    const ScreenLayout layout = screenLayout(ctx);
    const Rect& sceneRect = layout.scene;
    ctx.renderer->setSceneViewport(static_cast<u32>(sceneRect.x), static_cast<u32>(sceneRect.y), static_cast<u32>(sceneRect.width), static_cast<u32>(sceneRect.height));
    const Mat4 viewProjection = sceneViewProjection(ctx);

    // The UV workspace shows only the object being UV-edited: no other objects, reference images, or markers
    const bool uvWorkspace = ctx.workspace.current == Workspace::UV;
    const ObjectHandle uvTarget = uvObject(ctx);

    LightingState lighting = buildLightingState(ctx.scene.lights, ctx.viewport.headlight, ctx.scene.camera, ctx.renderer->getBackground());
    lighting.uvChecker = ctx.viewport.showUVChecker;
    ctx.renderer->setLighting(lighting);
    ctx.renderer->setExposure(ctx.viewport.exposure);

    ctx.objectMeshes.prune(ctx.scene.objects);
    ctx.pictureTextures.prune(*ctx.renderer);

    // Backdrop images go first so everything draws over them
    if (!uvWorkspace) drawReferenceImages(ctx, viewProjection, ReferenceDepth::Behind);

    // See-through objects and reference images among them wait until everything solid is drawn, then go farthest first
    struct SeeThrough {
        f32 distance;
        DrawCommand object;
        ReferenceHandle image = INVALID_REFERENCE;
    };
    std::vector<SeeThrough> seeThrough;

    const FaceGroupOf groupOf = faceGroupsFor(ctx);

    for (ObjectHandle handle : ctx.scene.objects.handles()) {
        if (uvWorkspace && handle != uvTarget) continue;
        Object& object = ctx.scene.objects.get(handle);

        Mat4 model = ctx.scene.objects.worldMatrix(handle);
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
            cmd.outlineAll = selection.hasObject(handle);
        } else if (active) {
            cmd.highlightedVerts = selection.getVertexHandles();
            cmd.highlightedEdges = selection.getEdgeHandles();
            cmd.highlightedFaces = selection.getFaceHandles();
            cmd.hardEdges = object.meshData.getHardEdges();
            if (uvWorkspace) cmd.seamEdges = object.meshData.getSeamEdges();

            // Selected faces are outlined with the same treatment as selected edges
            for (FaceHandle face : cmd.highlightedFaces) {
                const Face* data = object.meshData.getFace(face);
                if (!data) continue;
                for (EdgeHandle edge : object.meshData.getLoopEdges(data->edge)) cmd.highlightedEdges.push_back(edge);
            }
        }
        
        OpenGLMesh* mesh = ctx.objectMeshes.sync(handle, object, groupOf, ctx.scene.materials.stamp());
        cmd.mesh = mesh;
        cmd.model = model;
        cmd.mvp = mvp;

        // The object's own look tints selected faces; its faces draw by material (or all at once in clay view)
        const std::optional<SurfaceLook> surface = surfaceFor(ctx, object);
        cmd.surface = surface ? *surface : claySurface();
        ObjectParts parts = partsFor(ctx, object, *mesh);
        cmd.showFaces = parts.whole || !parts.solid.empty();
        cmd.parts = std::move(parts.solid);

        // See-through parts wait for everything solid; the wireframe and selection stay with the solid draw
        if (!parts.seeThrough.empty()) {
            const f32 distance = (worldCenter(ctx, handle) - ctx.scene.camera.position).length();
            DrawCommand faces;
            faces.mesh = mesh;
            faces.model = model;
            faces.mvp = mvp;
            faces.showVerts = faces.showEdges = false;
            faces.parts = std::move(parts.seeThrough);
            seeThrough.push_back({ distance, std::move(faces) });
        }

        ctx.renderer->draw(cmd);
    }

    for (ReferenceHandle handle : ctx.scene.references.handles()) {
        const ReferenceImage& image = ctx.scene.references.get(handle);
        if (!uvWorkspace && image.visible && image.depth == ReferenceDepth::InScene && image.opacity > 0.0f) {
            seeThrough.push_back({ (image.position - ctx.scene.camera.position).length(), {}, handle });
        }
    }

    std::sort(seeThrough.begin(), seeThrough.end(), [](const SeeThrough& a, const SeeThrough& b) { return a.distance > b.distance; });
    for (const SeeThrough& item : seeThrough) {
        if (!item.image.isNull()) drawReferenceImage(ctx, viewProjection, item.image);
        else ctx.renderer->draw(item.object);
    }

    DrawGridCommand gridCmd;
    gridCmd.viewProjection = viewProjection;
    gridCmd.cameraPosition = ctx.scene.camera.position;
    gridCmd.cameraDistance = ctx.scene.camera.distance;
    gridCmd.farPlane = ctx.scene.camera.farPlane;

    ctx.renderer->drawGrid(gridCmd);
    if (!uvWorkspace) drawReferenceImages(ctx, viewProjection, ReferenceDepth::InFront);

    if (ctx.systems.input_ctx.isActive(InputContext_Debug)) {
        ctx.debug_renderer.render(
            *ctx.renderer,
            ctx.scene,
            viewProjection
        );
    }

    UIDrawList& ui = ctx.uiDrawList;
    ui.clear();

    // Markers are clipped to the 3D view, so none spill into the top bar or the UV side
    if (!uvWorkspace) {
        ui.pushClip(sceneRect);
        drawReferenceOutlines(ctx, ui, viewProjection, sceneRect);
        drawLightMarkers(ctx, ui, viewProjection, sceneRect);
        drawParentLines(ctx, ui, viewProjection, sceneRect);
        // Origins win clicks over lights, so they draw over them too
        drawOriginMarkers(ctx, ui, viewProjection, sceneRect);
        ui.popClip();
    }
    drawToolGuides(ctx, ui);
    drawStatusBar(ctx, ui, static_cast<f32>(width), static_cast<f32>(height));
    drawStatsOverlay(ctx, ui);

    ctx.ui.setDrawList(&ui);
    ctx.ui.setFont(makeUIFont(FontId::UI, ctx.fonts.get(FontId::UI)));
    ctx.ui.beginDraw();
    drawWorkspaceChrome(ctx, layout);
    // The floating panel belongs to the Model workspace
    if (ctx.viewport.showPanel && ctx.workspace.current == Workspace::Model) drawMainPanel(ctx, layout.content);
    drawModal(ctx, { 0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height) });
    ctx.ui.endDraw();

    drawRadialMenu(ctx, ui);

    if (ctx.systems.input_ctx.isActive(InputContext_Console)) {
        drawConsole(ctx, ui, layout.content);
    }

    ctx.renderer->drawUI(ui);

    ctx.renderer->endMainPass();
    ctx.renderer->endFrame();
    // Before present, which waits for the screen when vsync is on
    ctx.frameStats.renderMilliseconds = std::chrono::duration<f32, std::milli>(std::chrono::steady_clock::now() - start).count();
    ctx.renderer->present();
}