#include "application/action_checks/action_checks.hpp"
#include "scene/selection/ray.hpp"
#include "scene/selection/scene_queries.hpp"

#include <algorithm>

void checkSelectionContext(AppContext& ctx) {
    ActionMap& actions = ctx.systems.actions;
    InputState& input = ctx.systems.input;
    ContextManager& ictx = ctx.systems.input_ctx;
    Camera& camera = ctx.scene.camera;

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

    // if (actions.wasActionPressedThisFrame(Action::Select, input, ictx.getContext())) {
    //     u32 width = 0;
    //     u32 height = 0;

    //     ctx.windows[0]->getDimensions(width, height);

    //     Ray ray = makeRayFromScreenPosition(
    //         input.getMouseX(),
    //         input.getMouseY(),
    //         width,
    //         height,
    //         camera
    //     );

    //     VertexHit hit = pickVertex(
    //         ctx.scene,
    //         ray,
    //         0.03f
    //     );

    //     bool addDown = actions.isActionDown(Action::AddSelection, input, ictx.getContext());
    //     bool removeDown = actions.isActionDown(Action::RemoveSelection, input, ictx.getContext());

    //     if (!addDown && !removeDown) {
    //         ctx.scene.selection.clear();
    //     }

    //     if (hit.hit) {
    //         if (removeDown) {
    //             ctx.scene.selection.removeVertex(
    //                 hit.objectIndex,
    //                 hit.vertexIndex
    //             );
    //         } else {
    //             ctx.scene.selection.addVertex(
    //                 hit.objectIndex,
    //                 hit.vertexIndex,
    //                 ctx.scene.objects.get(hit.objectIndex).meshData.getVertices()[hit.vertexIndex].position
    //             );
    //         }
    //     }
    // }

    // if (ctx.scene.selection.hasVertices() && actions.wasActionPressedThisFrame(Action::GrabSelection, input, ictx.getContext())) {
    //     ictx.setContext(InputContext_Grab);
    // }

    // if (actions.wasActionPressedThisFrame(Action::AddVertex, input, ictx.getContext())) {
    //     ctx.scene.objects.get(0).meshData.addVertex({ Vec3(0.0f, 0.0f, 0.0f) });
    //     ctx.scene.objects.get(0).meshDirty = true;
    // }

    // if (ctx.scene.selection.getVertices().size() == 2 && actions.wasActionPressedThisFrame(Action::AddEdge, input, ictx.getContext())) {
    //     auto& selectionVerts = ctx.scene.selection.getVertices();
    //     u32 vertIndex1 = selectionVerts[0].vertexIndex;
    //     u32 vertIndex2 = selectionVerts[1].vertexIndex;

    //     ctx.scene.objects.get(0).meshData.addEdge({ vertIndex1, vertIndex2 });
    //     ctx.scene.objects.get(0).meshDirty = true;
    // }

    // if (ctx.scene.selection.getVertices().size() >= 3 && actions.wasActionPressedThisFrame(Action::AddFace, input, ictx.getContext())) {
    //     auto& selectionVerts = ctx.scene.selection.getVertices();
    //     std::vector<u32> vertIndices;
    //     for(auto& selection : selectionVerts) {
    //         vertIndices.push_back(selection.vertexIndex);
    //     }

    //     std::vector<Edge> faceEdges;
    //     auto& edges = ctx.scene.objects.get(0).meshData.getEdges();
    //     for(u32 i = 0; i < (u32)edges.size(); i++) {
    //         const Edge& edge = edges[i];
    //         u32 matches = 0;
    //         for(u32 vertIndex : vertIndices) {
    //             if (edge.v0 == vertIndex) matches++;
    //             if (edge.v1 == vertIndex) matches++;
    //             if (matches == 2) break;
    //         }
    //         if (matches == 2) faceEdges.push_back(edge);
    //         if (faceEdges.size() == vertIndices.size()) break;
    //     }

    //     if (faceEdges.size() == vertIndices.size()) {
    //         bool loopConnected = false;
    //         std::vector<u32> faceVertsOrdered;
    //         u32 i = 0;
    //         while(!loopConnected) {
    //             if (i == 0) {
    //                 faceVertsOrdered.push_back(faceEdges[i].v0);
    //                 faceVertsOrdered.push_back(faceEdges[i].v1);
    //                 i++;
    //                 continue;
    //             }

    //             if (i >= 3) {
    //                 if (faceVertsOrdered.front() == faceVertsOrdered.back()) {
    //                     loopConnected = true;
    //                     continue;
    //                 }
    //             }

    //             const Edge edge = faceEdges[i % faceEdges.size()];
    //             if (edge.v0 == faceVertsOrdered.back()) {
    //                 faceVertsOrdered.push_back(edge.v1);
    //                 i++;
    //                 continue;
    //             }
    //             if (edge.v1 == faceVertsOrdered.back()) {
    //                 faceVertsOrdered.push_back(edge.v0);
    //                 i++;
    //                 continue;
    //             }
    //         }

    //         if ((faceVertsOrdered.size() - 1) == edges.size()) {
    //             std::vector<u32> gpuIndices;
    //             u32 triangleStart = faceVertsOrdered.front();

    //             u32 numberOfTriangles = (u32)(faceVertsOrdered.size() - 2);
    //             for(u32 i = 1; i < numberOfTriangles + 1; i++) {
    //                 gpuIndices.push_back(triangleStart);
    //                 gpuIndices.push_back(faceVertsOrdered[i]);
    //                 gpuIndices.push_back(faceVertsOrdered[i + 1]);
    //             }
                
    //             ctx.scene.objects.get(0).meshData.addFace({ gpuIndices });
    //             ctx.scene.objects.get(0).meshDirty = true;
    //         }
    //     }
    // }
}