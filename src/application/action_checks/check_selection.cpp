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
            EdgeHit hit = pickEdge(ctx.scene, ray, 0.03f);

            if (hit.hit) {
                const MeshData& mesh = ctx.scene.objects.get(hit.objectIndex).meshData;
                std::vector<VertexHandle> selectedVerts;
                selectedVerts.push_back(mesh.getEdgeOrigin(hit.edge));
                selectedVerts.push_back(mesh.getEdgeTip(hit.edge));

                if (removeDown) {
                    for (VertexHandle handle: selectedVerts) {
                        ctx.scene.selection.removeVertex(hit.objectIndex, handle);
                    }
                    ctx.scene.selection.removeEdge(hit.objectIndex, hit.edge);
                } else {
                    for (VertexHandle handle : selectedVerts) {
                        ctx.scene.selection.addVertex(hit.objectIndex, handle);
                    }
                    ctx.scene.selection.addEdge(hit.objectIndex, hit.edge);
                }
            }
        } else if (ictx.getSelectionContext() & InputContext_SelectionFace) {
            FaceHit hit = pickFace(ctx.scene, ray);

            if (hit.hit) {
                const MeshData& mesh = ctx.scene.objects.get(hit.objectIndex).meshData;
                std::vector<VertexHandle> selectedVerts = mesh.getFaceVertices(hit.face);
                if (removeDown) {
                    // TODO: removing a face also deselects vertices shared with other selected faces (same for edges).
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
        ictx.removeContext(InputContext_XAxis | InputContext_YAxis | InputContext_ZAxis);
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);
        ictx.setContext(InputContext_Grab);
    }

    if (ctx.scene.selection.getVertices().size() >= 2 && actions.wasActionPressedThisFrame(Action::ScaleSelection, input, ictx.getContext())) {
        ictx.removeContext(InputContext_XAxis | InputContext_YAxis | InputContext_ZAxis);
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);
        ictx.setContext(InputContext_Scale);
    }

    if (ctx.scene.selection.getVertices().size() >= 2 && actions.wasActionPressedThisFrame(Action::RotateSelection, input, ictx.getContext())) {
        ictx.removeContext(InputContext_XAxis | InputContext_YAxis | InputContext_ZAxis);
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);
        ictx.setContext(InputContext_Rotate);
    }

    if (actions.wasActionPressedThisFrame(Action::BevelSelection, input, ictx.getContext())) {
        beginBevel(ctx);
    }

    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionVertex) {
        if (actions.wasActionPressedThisFrame(Action::ConnectVertices, input, ictx.getContext())) {
            auto vertHandles = ctx.scene.selection.getVertexHandles();
            if (vertHandles.size() == 2) {
                ctx.scene.objects.get(0).meshData.connectVertices(vertHandles[0], vertHandles[1]);
                ctx.scene.objects.get(0).meshDirty = true;
            }
        }
    }

    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionEdge) {
        if (actions.wasActionPressedThisFrame(Action::FillFaceLoop, input, ictx.getContext())) {
            if (!ctx.scene.objects.get(0).meshData.getEdgeHandles().empty()) {
                ctx.scene.objects.get(0).meshData.fillFaceLoop(ctx.scene.selection.getEdgeHandles()[0]);
                ctx.scene.objects.get(0).meshDirty = true;
            }
        }
    }

    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionFace) {
        bool extruding = actions.wasActionPressedThisFrame(Action::ExtrudeSelection, input, ictx.getContext());
        bool insetting = actions.wasActionPressedThisFrame(Action::InsetSelection, input, ictx.getContext());
        if (extruding || insetting) {
            if (!ctx.scene.objects.get(0).meshData.getFaceHandles().empty()) {
                FaceHandle newFace = ctx.scene.objects.get(0).meshData.insertFaceRing(ctx.scene.selection.getFaceHandles()[0]);
        
                ctx.scene.selection.clear();
        
                std::vector<VertexHandle> newFaceVerts = ctx.scene.objects.get(0).meshData.getFaceVertices(newFace);
                
                ctx.scene.selection.addFace(0, newFace);
                for (VertexHandle vert : newFaceVerts) {
                    ctx.scene.selection.addVertex(0, vert);
                }
                
                std::vector<Vec3> starts;
                for (const auto handle : ctx.scene.selection.getVertexHandles()) {
                    starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
                }
                ctx.scene.selection.setSelectionStartPositions(starts);
                if (extruding) ictx.setContext(InputContext_Grab);
                else if (insetting) ictx.setContext(InputContext_Scale);
            }
        }
    }

    if (actions.wasActionPressedThisFrame(Action::DeleteSelection, input, ictx.getContext())) {
        u32 selectionCtx = ctx.systems.input_ctx.getSelectionContext();

        if (selectionCtx == InputContext_SelectionVertex) {
            for (VertexHandle vert : ctx.scene.selection.getVertexHandles()) {
                ctx.scene.objects.get(0).meshData.removeVertex(vert);
            }
        } else if (selectionCtx == InputContext_SelectionEdge) {
            for (EdgeHandle edge : ctx.scene.selection.getEdgeHandles()) {
                ctx.scene.objects.get(0).meshData.removeEdge(edge);
            }
        } else {
            for (FaceHandle face : ctx.scene.selection.getFaceHandles()) {
                ctx.scene.objects.get(0).meshData.removeFace(face);
            }
        }

        ctx.scene.objects.get(0).meshDirty = true;
        ctx.scene.selection.clear();
    }
}