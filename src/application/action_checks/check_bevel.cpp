#include "application/action_checks/action_checks.hpp"
#include "core/math/vec4.hpp"

#include <cmath>
#include <iostream>

namespace {
    bool projectToScreen(AppContext& ctx, const Object& object, Vec3 point, f32& x, f32& y, f32& worldPerPixel) {
        u32 width = 0;
        u32 height = 0;
        ctx.windows[0]->getDimensions(width, height);

        if (width == 0 || height == 0) return false;

        const Camera& camera = ctx.scene.camera;
        const f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);

        const Mat4 mvp =
            camera.getProjectionMatrix(aspectRatio) *
            camera.getViewMatrix() *
            object.transform.getMatrix();

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
    BevelTool& bevel = ctx.bevel;
    const u32 mode = ctx.systems.input_ctx.getSelectionContext();

    bool started = false;

    if (mode == InputContext_SelectionVertex) {
        if (selection.getVertices().size() != 1) return;

        const VertexSelection& selected = selection.getVertices()[0];
        MeshData& mesh = ctx.scene.objects.get(selected.objectIndex).meshData;

        bevel.objectIndex = selected.objectIndex;
        bevel.pivot = mesh.getVertexPosition(selected.vertex);
        started = mesh.bevelVertex(selected.vertex, bevel.session);
    } else if (mode == InputContext_SelectionEdge) {
        if (selection.getEdges().size() != 1) return;

        const EdgeSelection& selected = selection.getEdges()[0];
        MeshData& mesh = ctx.scene.objects.get(selected.objectIndex).meshData;

        bevel.objectIndex = selected.objectIndex;
        bevel.pivot =
            (mesh.getVertexPosition(mesh.getEdgeOrigin(selected.edge)) +
             mesh.getVertexPosition(mesh.getEdgeTip(selected.edge))) / 2.0f;
        started = mesh.bevelEdge(selected.edge, bevel.session);
    } else if (mode == InputContext_SelectionFace) {
        if (selection.getFaces().size() != 1) return;

        const FaceSelection& selected = selection.getFaces()[0];
        MeshData& mesh = ctx.scene.objects.get(selected.objectIndex).meshData;

        const std::vector<VertexHandle> corners = mesh.getFaceVertices(selected.face);
        Vec3 center(0.0f);
        for (VertexHandle corner : corners) center += mesh.getVertexPosition(corner);

        bevel.objectIndex = selected.objectIndex;
        bevel.pivot = center / static_cast<f32>(corners.size());
        started = mesh.bevelFace(selected.face, bevel.session);
    }

    if (!started) {
        std::cout << "bevel: can't bevel this (borders aren't supported yet)" << std::endl;
        return;
    }

    Object& object = ctx.scene.objects.get(bevel.objectIndex);

    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 worldPerPixel = 0.0f;

    bevel.startDistance = projectToScreen(ctx, object, bevel.pivot, x, y, worldPerPixel)
        ? mouseDistance(ctx, x, y)
        : 0.0f;

    bevel.savedSelection = selection;
    selection.clear();

    object.meshDirty = true;

    ctx.systems.input_ctx.setContext(InputContext_Bevel);
}

void checkBevelContext(AppContext& ctx) {
    BevelTool& bevel = ctx.bevel;
    Object& object = ctx.scene.objects.get(bevel.objectIndex);

    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 worldPerPixel = 0.0f;

    if (projectToScreen(ctx, object, bevel.pivot, x, y, worldPerPixel)) {
        const f32 width = (mouseDistance(ctx, x, y) - bevel.startDistance) * worldPerPixel;

        object.meshData.setBevelWidth(bevel.session, width);
        object.meshDirty = true;
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::ConfirmBevel,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }

    if (ctx.systems.actions.wasActionPressedThisFrame(
            Action::CancelBevel,
            ctx.systems.input,
            ctx.systems.input_ctx.getContext())) {

        object.meshData.cancelBevel(bevel.session);
        object.meshDirty = true;

        ctx.scene.selection = bevel.savedSelection;

        ctx.systems.input_ctx.setContext(
            ctx.systems.input_ctx.getSelectionContext()
        );
    }
}
