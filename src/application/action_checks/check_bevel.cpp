#include "application/action_checks/action_checks.hpp"
#include "core/math/vec4.hpp"

#include <cmath>
#include <iostream>

namespace {
    bool projectToScreen(AppContext& ctx, const Mat4& model, Vec3 point, f32& x, f32& y, f32& worldPerPixel) {
        u32 width = 0;
        u32 height = 0;
        ctx.windows[0]->getDimensions(width, height);

        if (width == 0 || height == 0) return false;

        const Camera& camera = ctx.scene.camera;
        const f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);

        const Mat4 mvp =
            camera.getProjectionMatrix(aspectRatio) *
            camera.getViewMatrix() *
            model;

        const Vec4 clip = mvp * Vec4(point.x, point.y, point.z, 1.0f);

        // For a perspective projection, w is the depth in front of the camera.
        if (clip.w <= 1e-6f) return false;

        x = (clip.x / clip.w + 1.0f) * 0.5f * static_cast<f32>(width);
        y = (1.0f - clip.y / clip.w) * 0.5f * static_cast<f32>(height);

        worldPerPixel = 2.0f * clip.w * std::tan(camera.fovRadians * 0.5f) / static_cast<f32>(height);

        return true;
    }

    f32 mouseDistance(AppContext& ctx, f32 x, f32 y) {
        return std::hypot(
            static_cast<f32>(ctx.systems.input.getMouseX()) - x,
            static_cast<f32>(ctx.systems.input.getMouseY()) - y
        );
    }
}

void beginBevel(AppContext& ctx) {
    Selection& selection = ctx.scene.selection;
    WidthTool& bevel = ctx.widthTool;
    const u32 mode = ctx.systems.input_ctx.getSelectionContext();

    bool started = false;

    if (mode == InputContext_SelectionVertex) {
        if (selection.getVertices().size() != 1) return;

        const VertexSelection& selected = selection.getVertices()[0];
        MeshData& mesh = ctx.scene.objects.get(selected.object).meshData;

        bevel.object = selected.object;
        bevel.pivot = mesh.getVertexPosition(selected.vertex);
        ctx.history.begin(ctx.scene);
        started = mesh.bevelVertex(selected.vertex, bevel.session, ctx.scene.objects.worldMatrix(selected.object));
    } else if (mode == InputContext_SelectionEdge) {
        if (selection.getEdges().size() != 1) return;

        const EdgeSelection& selected = selection.getEdges()[0];
        MeshData& mesh = ctx.scene.objects.get(selected.object).meshData;

        bevel.object = selected.object;
        bevel.pivot =
            (mesh.getVertexPosition(mesh.getEdgeOrigin(selected.edge)) +
             mesh.getVertexPosition(mesh.getEdgeTip(selected.edge))) / 2.0f;
        ctx.history.begin(ctx.scene);
        started = mesh.bevelEdge(selected.edge, bevel.session, ctx.scene.objects.worldMatrix(selected.object));
    } else if (mode == InputContext_SelectionFace) {
        if (selection.getFaces().size() != 1) return;

        const FaceSelection& selected = selection.getFaces()[0];
        MeshData& mesh = ctx.scene.objects.get(selected.object).meshData;

        const std::vector<VertexHandle> corners = mesh.getFaceVertices(selected.face);
        Vec3 center(0.0f);
        for (VertexHandle corner : corners) center += mesh.getVertexPosition(corner);

        bevel.object = selected.object;
        bevel.pivot = center / static_cast<f32>(corners.size());
        ctx.history.begin(ctx.scene);
        started = mesh.bevelFace(selected.face, bevel.session, ctx.scene.objects.worldMatrix(selected.object));
    }

    if (!started) {
        ctx.history.cancel(ctx.scene);
        ctx.systems.console.printError("bevel: can't bevel this (the corner is too complex, e.g. two separate open edges meet there)");
        return;
    }

    Object& object = ctx.scene.objects.get(bevel.object);

    bevel.startMouseX = static_cast<f32>(ctx.systems.input.getMouseX());
    bevel.startMouseY = static_cast<f32>(ctx.systems.input.getMouseY());

    bevel.savedSelection = selection;
    selection.clear();

    object.meshDirty = true;

    ctx.systems.input_ctx.setContext(InputContext_Bevel);
}

void checkBevelContext(AppContext& ctx) {
    updateWidthTool(ctx, Action::ConfirmBevel, Action::CancelBevel);
}

void updateWidthTool(AppContext& ctx, Action confirm, Action cancel) {
    WidthTool& bevel = ctx.widthTool;
    Object& object = ctx.scene.objects.get(bevel.object);

    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 worldPerPixel = 0.0f;

    if (projectToScreen(ctx, ctx.scene.objects.worldMatrix(bevel.object), bevel.pivot, x, y, worldPerPixel)) {
        const f32 width = mouseDistance(ctx, bevel.startMouseX, bevel.startMouseY) * worldPerPixel;

        object.meshData.setSlideWidth(bevel.session, width);
        object.meshDirty = true;
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
            confirm,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        ctx.history.commit();

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
            cancel,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        object.meshData.cancelSlide(bevel.session);
        object.meshDirty = true;

        ctx.scene.selection = bevel.savedSelection;

        ctx.history.cancel(ctx.scene);

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }
}
