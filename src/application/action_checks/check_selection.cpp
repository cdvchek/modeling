#include "application/action_checks/action_checks.hpp"
#include "scene/selection/ray.hpp"
#include "scene/selection/scene_queries.hpp"
#include "application/light_markers.hpp"

#include "core/math/vec4.hpp"

#include <algorithm>

namespace {
    Vec3 toWorld(const Object& object, Vec3 point) {
        const Vec4 world = object.transform.getMatrix() * Vec4(point.x, point.y, point.z, 1.0f);
        return Vec3(world.x, world.y, world.z);
    }

    f32 distanceToSegment(Vec3 point, Vec3 a, Vec3 b) {
        const Vec3 ab = b - a;
        const f32 lengthSq = Vec3::dot(ab, ab);
        const f32 t = lengthSq > 0.0f ? std::clamp(Vec3::dot(point - a, ab) / lengthSq, 0.0f, 1.0f) : 0.0f;
        return (point - (a + ab * t)).length();
    }

    bool isEdgeSelected(const Selection& selection, u32 object, const MeshData& mesh, EdgeHandle edge) {
        return selection.hasEdge(object, edge) || selection.hasEdge(object, mesh.getEdge(edge)->pair);
    }

    void selectEdge(Selection& selection, u32 object, const MeshData& mesh, EdgeHandle edge) {
        if (isEdgeSelected(selection, object, mesh, edge)) return;

        selection.addEdge(object, edge);
        selection.addVertex(object, mesh.getEdgeOrigin(edge));
        selection.addVertex(object, mesh.getEdgeTip(edge));
    }

    void selectFace(Selection& selection, u32 object, const MeshData& mesh, FaceHandle face) {
        selection.addFace(object, face);

        for (VertexHandle vertex : mesh.getFaceVertices(face)) {
            selection.addVertex(object, vertex);
        }
    }

    // Deselect the vertices that no remaining selected edge or face uses.
    void deselectUnusedVertices(Selection& selection, u32 object, const MeshData& mesh, const std::vector<VertexHandle>& vertices) {
        for (VertexHandle vertex : vertices) {
            bool used = false;

            for (EdgeHandle edge : selection.getEdgeHandles()) {
                if (mesh.getEdgeOrigin(edge) == vertex || mesh.getEdgeTip(edge) == vertex) used = true;
            }

            for (FaceHandle face : selection.getFaceHandles()) {
                const std::vector<VertexHandle> corners = mesh.getFaceVertices(face);
                if (std::find(corners.begin(), corners.end(), vertex) != corners.end()) used = true;
            }

            if (!used) selection.removeVertex(object, vertex);
        }
    }

    void deselectEdge(Selection& selection, u32 object, const MeshData& mesh, EdgeHandle edge) {
        selection.removeEdge(object, edge);
        selection.removeEdge(object, mesh.getEdge(edge)->pair);
        deselectUnusedVertices(selection, object, mesh, { mesh.getEdgeOrigin(edge), mesh.getEdgeTip(edge) });
    }

    void deselectFace(Selection& selection, u32 object, const MeshData& mesh, FaceHandle face) {
        selection.removeFace(object, face);
        deselectUnusedVertices(selection, object, mesh, mesh.getFaceVertices(face));
    }

    void selectLoop(AppContext& ctx, const Ray& ray, bool ring) {
        Selection& selection = ctx.scene.selection;

        if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionEdge) {
            EdgeHit hit = pickEdge(ctx.scene, ray, 0.03f);
            if (!hit.hit) return;

            const MeshData& mesh = ctx.scene.objects.get(hit.objectIndex).meshData;

            selection.clearLights();

            for (EdgeHandle edge : ring ? mesh.getEdgeRing(hit.edge) : mesh.getEdgeLoop(hit.edge)) {
                selectEdge(selection, hit.objectIndex, mesh, edge);
            }
        } else if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionFace) {
            FaceHit hit = pickFace(ctx.scene, ray);
            if (!hit.hit) return;

            const Object& object = ctx.scene.objects.get(hit.objectIndex);
            const MeshData& mesh = object.meshData;

            // The loop runs across the clicked face's edge nearest the click.
            const Vec3 point = ray.origin + ray.direction * hit.distance;

            EdgeHandle nearest = INVALID_EDGE;
            f32 nearestDistance = 0.0f;

            for (EdgeHandle edge : mesh.getLoopEdges(mesh.getFace(hit.face)->edge)) {
                const f32 distance = distanceToSegment(
                    point,
                    toWorld(object, mesh.getVertexPosition(mesh.getEdgeOrigin(edge))),
                    toWorld(object, mesh.getVertexPosition(mesh.getEdgeTip(edge)))
                );

                if (nearest.isNull() || distance < nearestDistance) {
                    nearest = edge;
                    nearestDistance = distance;
                }
            }

            selection.clearLights();

            for (FaceHandle face : mesh.getFaceLoop(nearest)) {
                selectFace(selection, hit.objectIndex, mesh, face);
            }
        }
    }
}

void checkSelectionContext(AppContext& ctx) {
    ActionMap& actions = ctx.systems.actions;
    InputState& input = ctx.systems.input;
    ContextManager& ictx = ctx.systems.input_ctx;
    Camera& camera = ctx.scene.camera;

    if (actions.wasActionPressedThisFrame(Action::Undo, input, ictx.getContext())) ctx.history.undo(ctx.scene);
    if (actions.wasActionPressedThisFrame(Action::Redo, input, ictx.getContext())) ctx.history.redo(ctx.scene);

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

    const bool selectingLoop = actions.wasActionPressedThisFrame(Action::SelectLoop, input, ictx.getContext());
    const bool selectingRing = actions.wasActionPressedThisFrame(Action::SelectRing, input, ictx.getContext());

    if (selectingLoop || selectingRing) {
        u32 width = 0;
        u32 height = 0;

        ctx.windows[0]->getDimensions(width, height);

        Ray ray = makeRayFromScreenPosition(input.getMouseX(), input.getMouseY(), width, height, camera);
        selectLoop(ctx, ray, selectingRing);
    } else if (actions.wasActionPressedThisFrame(Action::Select, input, ictx.getContext())) {
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

        Selection& selection = ctx.scene.selection;
        const bool toggling = actions.isActionDown(Action::ToggleSelection, input, ictx.getContext());

        if (!toggling) selection.clear();

        // Light markers sit on top of the scene, so they're picked before mesh elements
        const Mat4 viewProjection = camera.getProjectionMatrix(static_cast<f32>(width) / static_cast<f32>(height)) * camera.getViewMatrix();
        const LightHit lightHit = pickLight(ctx.scene, viewProjection, static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()),
                                           static_cast<f32>(width), static_cast<f32>(height), LIGHT_MARKER_PICK_RADIUS);

        if (lightHit.hit) {
            selection.clearMeshElements();

            if (toggling && selection.hasLight(lightHit.light)) {
                selection.removeLight(lightHit.light);
            } else {
                selection.addLight(lightHit.light);
            }
        } else if (ictx.getSelectionContext() & InputContext_SelectionVertex) {
            VertexHit hit = pickVertex(ctx.scene, ray, 0.03f);

            if (hit.hit) {
                selection.clearLights();

                if (toggling && selection.hasVertex(hit.objectIndex, hit.vertex)) {
                    selection.removeVertex(hit.objectIndex, hit.vertex);
                } else {
                    selection.addVertex(hit.objectIndex, hit.vertex);
                }
            }
        } else if (ictx.getSelectionContext() & InputContext_SelectionEdge) {
            EdgeHit hit = pickEdge(ctx.scene, ray, 0.03f);

            if (hit.hit) {
                selection.clearLights();

                const MeshData& mesh = ctx.scene.objects.get(hit.objectIndex).meshData;

                if (toggling && isEdgeSelected(selection, hit.objectIndex, mesh, hit.edge)) {
                    deselectEdge(selection, hit.objectIndex, mesh, hit.edge);
                } else {
                    selectEdge(selection, hit.objectIndex, mesh, hit.edge);
                }
            }
        } else if (ictx.getSelectionContext() & InputContext_SelectionFace) {
            FaceHit hit = pickFace(ctx.scene, ray);

            if (hit.hit) {
                selection.clearLights();

                const MeshData& mesh = ctx.scene.objects.get(hit.objectIndex).meshData;

                if (toggling && selection.hasFace(hit.objectIndex, hit.face)) {
                    deselectFace(selection, hit.objectIndex, mesh, hit.face);
                } else {
                    selectFace(selection, hit.objectIndex, mesh, hit.face);
                }
            }
        }
    }

    const bool grabbable = ctx.scene.selection.hasVertices() || ctx.scene.selection.hasLights();
    if (grabbable && actions.wasActionPressedThisFrame(Action::GrabSelection, input, ictx.getContext())) {
        ictx.removeContext(InputContext_XAxis | InputContext_YAxis | InputContext_ZAxis);
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);

        std::vector<Vec3> lightStarts;
        for (LightHandle light : ctx.scene.selection.getLights()) {
            const Light* data = ctx.scene.lights.tryGet(light);
            lightStarts.push_back(data ? data->position : Vec3());
        }
        ctx.scene.selection.setLightStartPositions(lightStarts);
        ctx.history.begin(ctx.scene);
        ictx.setContext(InputContext_Grab);
    }

    if (ctx.scene.selection.getVertices().size() >= 2 && actions.wasActionPressedThisFrame(Action::ScaleSelection, input, ictx.getContext())) {
        ictx.removeContext(InputContext_XAxis | InputContext_YAxis | InputContext_ZAxis);
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);
        ctx.history.begin(ctx.scene);
        ictx.setContext(InputContext_Scale);
    }

    const bool rotatable = ctx.scene.selection.getVertices().size() >= 2 || ctx.scene.selection.hasLights();
    if (rotatable && actions.wasActionPressedThisFrame(Action::RotateSelection, input, ictx.getContext())) {
        ictx.removeContext(InputContext_XAxis | InputContext_YAxis | InputContext_ZAxis);
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.objects.get(0).meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);

        std::vector<Vec3> lightDirections;
        for (LightHandle light : ctx.scene.selection.getLights()) {
            const Light* data = ctx.scene.lights.tryGet(light);
            lightDirections.push_back(data ? data->direction : Vec3(0.0f, -1.0f, 0.0f));
        }
        ctx.scene.selection.setLightStartDirections(lightDirections);
        ctx.history.begin(ctx.scene);
        ictx.setContext(InputContext_Rotate);
    }

    if (actions.wasActionPressedThisFrame(Action::BevelSelection, input, ictx.getContext())) {
        beginBevel(ctx);
    }

    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionVertex) {
        if (actions.wasActionPressedThisFrame(Action::ConnectVertices, input, ictx.getContext())) {
            auto vertHandles = ctx.scene.selection.getVertexHandles();
            if (vertHandles.size() == 2) {
                ctx.history.begin(ctx.scene);

                if (ctx.scene.objects.get(0).meshData.connectVertices(vertHandles[0], vertHandles[1])) ctx.history.commit();
                else ctx.history.cancel(ctx.scene);

                ctx.scene.objects.get(0).meshDirty = true;
            }
        }
    }

    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionEdge) {
        if (actions.wasActionPressedThisFrame(Action::FillFaceLoop, input, ictx.getContext())) {
            if (!ctx.scene.selection.getEdgeHandles().empty()) {
                ctx.history.begin(ctx.scene);

                if (ctx.scene.objects.get(0).meshData.fillFaceLoop(ctx.scene.selection.getEdgeHandles()[0])) ctx.history.commit();
                else ctx.history.cancel(ctx.scene);

                ctx.scene.objects.get(0).meshDirty = true;
            }
        }
    }

    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionFace) {
        bool extruding = actions.wasActionPressedThisFrame(Action::ExtrudeSelection, input, ictx.getContext());
        bool insetting = actions.wasActionPressedThisFrame(Action::InsetSelection, input, ictx.getContext());
        if (extruding || insetting) {
            if (!ctx.scene.selection.getFaceHandles().empty()) {
                ctx.history.begin(ctx.scene);

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

    if (ctx.scene.selection.hasVertices() && actions.wasActionPressedThisFrame(Action::DeleteSelection, input, ictx.getContext())) {
        ctx.history.begin(ctx.scene);

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

        ctx.history.commit();
    }
}