#include "application/action_checks/action_checks.hpp"
#include "scene/selection/ray.hpp"
#include "scene/selection/scene_queries.hpp"

#include <algorithm>

void checkSelectionContext(AppContext& ctx) {
    ActionMap& actions = ctx.systems.actions;
    InputState& input = ctx.systems.input;
    ContextManager& ictx = ctx.systems.input_ctx;
    Camera& camera = ctx.scene.camera;

    if (actions.wasActionPressedThisFrame(Action::VertexMode, input, ictx.getContext())) {
        ictx.setSelectionContext(InputContext_SelectionVertex);
        ctx.scene.selection.clear();
    }

    if (actions.wasActionPressedThisFrame(Action::EdgeMode, input, ictx.getContext())) {
        ictx.setSelectionContext(InputContext_SelectionEdge);
        ctx.scene.selection.clear();
    }

    if (actions.wasActionPressedThisFrame(Action::FaceMode, input, ictx.getContext())) {
        ictx.setSelectionContext(InputContext_SelectionFace);
        ctx.scene.selection.clear();
    }

    if (actions.isActionDown(Action::ViewportOrbit, input, ictx.getContext())) {
        i32 dx = input.getMouseDeltaX();
        i32 dy = input.getMouseDeltaY();

        dx = std::clamp(dx, -500, 500);
        dy = std::clamp(dy, -500, 500);

        camera.yaw -= (f32)dx * 0.005f;
        camera.pitch += (f32)dy * 0.005f;

        camera.pitch = std::clamp(camera.pitch, -1.552f, 1.552f);
        camera.updatePositionFromOrbit();
    }

    if (actions.isActionDown(Action::ViewportPan, input, ictx.getContext())) {
        Vec3 right = camera.getRight();
        Vec3 cameraUp = Vec3::cross(right, camera.getForward()).normalized();

        f32 panSpeed = 0.001f * camera.distance;

        Vec3 pan = (-right * input.getMouseDeltaX() + cameraUp * input.getMouseDeltaY()) * panSpeed;
        
        camera.position += pan;
        camera.target += pan;
    }

    i32 zoom = 0;
    if (actions.isActionDown(Action::ViewportZoom, input, ictx.getContext(), &zoom)) {
        camera.distance -= zoom * 0.005f;
        if (camera.distance <= 0.5) camera.distance = 0.5;
        camera.updatePositionFromOrbit();
    }

    if (actions.wasActionPressedThisFrame(Action::Select, input, ictx.getContext())) {
        u32 width = 0;
        u32 height = 0;

        ctx.windows[0]->getDimensions(width, height);

        Ray ray = makeRayFromScreenPosition(
            input.getMouseX(),
            input.getMouseY(),
            width,
            height,
            camera
        );

        bool addDown = actions.isActionDown(Action::AddSelection, input, ictx.getContext());
        bool removeDown = actions.isActionDown(Action::RemoveSelection, input, ictx.getContext());

        if (!addDown && !removeDown) {
            ctx.scene.selection.clear();
        }

        if (ictx.getSelectionContext() & InputContext_SelectionVertex) {
            VertexHit hit = pickVertex(ctx.scene, ray, 0.03f);
    
            if (hit.hit) {
                if (removeDown) {
                    ctx.scene.selection.removeVertex(
                        hit.objectIndex,
                        hit.vertex
                    );
                } else {
                    ctx.scene.selection.addVertex(
                        hit.objectIndex,
                        hit.vertex
                    );
                }
            }
        } else if (ictx.getSelectionContext() & InputContext_SelectionEdge) {

        } else if (ictx.getSelectionContext() & InputContext_SelectionFace) {
            FaceHit hit = pickFace(ctx.scene, ray);

            if (hit.hit) {
                const MeshData& mesh = ctx.scene.objects.get(hit.objectIndex).meshData;
                std::vector<VertexHandle> selectedVerts = mesh.getFaceVertices(hit.face);
                //std::vector<u32> selectedEdges = mesh.getFaceEdges(hit.faceIndex);
                if (removeDown) {
                    // TODO: current bug, if you remove a face from the selection, all of the adjacent selected faces will only be partially selected.
                    // the vertices that the adjacent faces share with the deselected face will be deselected and they shouldn't.
                    for (VertexHandle handle : selectedVerts) {
                        ctx.scene.selection.removeVertex(hit.objectIndex, handle);
                    }
                    ctx.scene.selection.removeFace(hit.objectIndex, hit.face);
                } else {
                    for (VertexHandle handle : selectedVerts) {
                        ctx.scene.selection.addVertex(hit.objectIndex, handle);
                    }
                    ctx.scene.selection.addFace(hit.objectIndex, hit.face);
                }
            }
        }
    }

    if (ctx.scene.selection.hasVertices() && actions.wasActionPressedThisFrame(Action::GrabSelection, input, ictx.getContext())) {
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);
        ictx.setContext(InputContext_Grab);
    }

    if (ctx.scene.selection.getVertices().size() >= 2 && actions.wasActionPressedThisFrame(Action::ScaleSelection, input, ictx.getContext())) {
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);
        ictx.setContext(InputContext_Scale);
    }

    // if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionFace && actions.wasActionPressedThisFrame(Action::ExtrudeSelection, input, ictx.getContext())) {
    //     // insert the new additions to the mesh
    //     // then act like the selection is just being grabbed on the axis of the normal of the face

    //     ctx.scene.objects.get(0).meshData.insertExtrusion(ctx.scene.selection.getFaceHandles()[0]);

    //     std::vector<Vec3> starts;
    //     for (const auto handle : ctx.scene.selection.getVertexHandles()) {
    //         starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
    //     }
    //     ctx.scene.selection.setSelectionStartPositions(starts);
    //     ictx.setContext(InputContext_Grab);
    // }
}