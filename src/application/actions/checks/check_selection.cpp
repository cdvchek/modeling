#include "application/actions/checks/action_checks.hpp"
#include "application/workspace.hpp"
#include "scene/picking/ray.hpp"
#include "scene/picking/scene_queries.hpp"
#include "scene/selection/mesh_selection.hpp"
#include "application/viewport/light_markers.hpp"
#include "application/viewport/origin_markers.hpp"
#include "application/viewport/material_view.hpp"

#include "core/math/vec4.hpp"

#include <algorithm>
#include <chrono>
#include <cfloat>

namespace {
    // In material view, faces seen from behind on single-sided materials aren't drawn, so they aren't clicked either
    BackFacesCulled culledFaces(const AppContext& ctx) {
        return [&ctx](ObjectHandle handle) { return backFacesCulled(ctx, handle); };
    }

    Vec3 toWorld(const Mat4& model, Vec3 point) {
        const Vec4 world = model * Vec4(point.x, point.y, point.z, 1.0f);
        return Vec3(world.x, world.y, world.z);
    }

    f32 distanceToSegment(Vec3 point, Vec3 a, Vec3 b) {
        const Vec3 ab = b - a;
        const f32 lengthSq = Vec3::dot(ab, ab);
        const f32 t = lengthSq > 0.0f ? std::clamp(Vec3::dot(point - a, ab) / lengthSq, 0.0f, 1.0f) : 0.0f;
        return (point - (a + ab * t)).length();
    }

    void selectLoop(AppContext& ctx, const Ray& ray, bool ring) {
        Selection& selection = ctx.scene.selection;

        if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionEdge) {
            EdgeHit hit = pickEdge(ctx.scene, ray, 0.03f, selection.getActiveObject());
            if (!hit.hit) return;

            const MeshData& mesh = ctx.scene.objects.get(hit.object).meshData;

            selection.clearLights();

            for (EdgeHandle edge : ring ? mesh.getEdgeRing(hit.edge) : mesh.getEdgeLoop(hit.edge)) {
                selectEdge(selection, hit.object, mesh, edge);
            }
        } else if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionFace) {
            FaceHit hit = pickFace(ctx.scene, ray, selection.getActiveObject(), INVALID_OBJECT, culledFaces(ctx));
            if (!hit.hit) return;

            const Object& object = ctx.scene.objects.get(hit.object);
            const Mat4 model = ctx.scene.objects.worldMatrix(hit.object);
            const MeshData& mesh = object.meshData;

            // The loop runs across the clicked face's edge nearest the click.
            const Vec3 point = ray.origin + ray.direction * hit.distance;

            EdgeHandle nearest = INVALID_EDGE;
            f32 nearestDistance = 0.0f;

            for (EdgeHandle edge : mesh.getLoopEdges(mesh.getFace(hit.face)->edge)) {
                const f32 distance = distanceToSegment(
                    point,
                    toWorld(model, mesh.getVertexPosition(mesh.getEdgeOrigin(edge))),
                    toWorld(model, mesh.getVertexPosition(mesh.getEdgeTip(edge)))
                );

                if (nearest.isNull() || distance < nearestDistance) {
                    nearest = edge;
                    nearestDistance = distance;
                }
            }

            selection.clearLights();

            for (FaceHandle face : mesh.getFaceLoop(nearest)) {
                selectFace(selection, hit.object, mesh, face);
            }
        }
    }
}

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

    const bool selectingLoop = actions.wasActionPressedThisFrame(Action::SelectLoop, input, ictx.getContext());
    const bool selectingRing = actions.wasActionPressedThisFrame(Action::SelectRing, input, ictx.getContext());

    // Clicks are measured inside the 3D view, which may not start at the window's corner
    const Rect view = sceneView(ctx);
    const f32 viewMouseX = static_cast<f32>(input.getMouseX()) - view.x;
    const f32 viewMouseY = static_cast<f32>(input.getMouseY()) - view.y;
    const u32 viewWidth = static_cast<u32>(view.width);
    const u32 viewHeight = static_cast<u32>(view.height);

    if (selectingLoop || selectingRing) {
        Ray ray = makeRayFromScreenPosition(static_cast<i32>(viewMouseX), static_cast<i32>(viewMouseY), viewWidth, viewHeight, camera);
        selectLoop(ctx, ray, selectingRing);
    } else if (actions.wasActionPressedThisFrame(Action::Select, input, ictx.getContext())) {
        Ray ray = makeRayFromScreenPosition(static_cast<i32>(viewMouseX), static_cast<i32>(viewMouseY), viewWidth, viewHeight, camera);

        // Times the click's picking and selecting, for the stats readout
        struct PickTimer {
            f32& result;
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            ~PickTimer() { result = std::chrono::duration<f32, std::milli>(std::chrono::steady_clock::now() - start).count(); }
        } pickTimer { ctx.frameStats.lastPickMilliseconds };

        Selection& selection = ctx.scene.selection;
        const bool toggling = actions.isActionDown(Action::ToggleSelection, input, ictx.getContext());

        if (!toggling) selection.clear();

        // Markers sit on top of the scene, so they're picked first: origins, then lights, then reference images and
        // mesh elements by which is in front
        const Mat4 viewProjection = sceneViewProjection(ctx);
        // The UV workspace shows only its one object, so markers, images, and other objects can't be clicked there
        const bool uvWorkspace = ctx.workspace.current == Workspace::UV;
        const OriginHit originHit = ctx.viewport.showOrigins && !uvWorkspace
            ? pickOrigin(ctx.scene, viewProjection, viewMouseX, viewMouseY, view.width, view.height, ORIGIN_MARKER_PICK_RADIUS)
            : OriginHit {};
        const LightHit lightHit = originHit.hit || uvWorkspace ? LightHit {}
            : pickLight(ctx.scene, viewProjection, viewMouseX, viewMouseY, view.width, view.height, LIGHT_MARKER_PICK_RADIUS);

        // A reference image wins when it's drawn over the mesh under the mouse
        ReferenceHit referenceHit = (originHit.hit || lightHit.hit || uvWorkspace) ? ReferenceHit {} : pickReference(ctx.scene, ray);
        if (referenceHit.hit && referenceHit.depth != ReferenceDepth::InFront) {
            const FaceHit meshHit = pickFace(ctx.scene, ray, INVALID_OBJECT, INVALID_OBJECT, culledFaces(ctx));
            const bool hidden = meshHit.hit && (referenceHit.depth == ReferenceDepth::Behind || meshHit.distance < referenceHit.distance);
            if (hidden) referenceHit = {};
        }

        if (originHit.hit) {
            // Another object's origin makes that object the one being edited (or the active one), with nothing else selected
            if (toggling && selection.getOrigin() == originHit.object) {
                selection.clearOrigin();
            } else {
                selection.clear();
                selection.selectOrigin(originHit.object);
            }
        } else if (referenceHit.hit) {
            if (toggling && selection.hasReference(referenceHit.reference)) {
                selection.removeReference(referenceHit.reference);
            } else {
                selection.addReference(referenceHit.reference);
            }
        } else if (lightHit.hit) {
            selection.clearMeshElements();
            selection.clearObjects();

            if (toggling && selection.hasLight(lightHit.light)) {
                selection.removeLight(lightHit.light);
            } else {
                selection.addLight(lightHit.light);
            }
        } else if (ictx.getSelectionContext() == InputContext_SelectionObject) {
            // Object mode picks whole objects; the last one clicked becomes the active object
            const FaceHit objectHit = pickFace(ctx.scene, ray, INVALID_OBJECT, INVALID_OBJECT, culledFaces(ctx));

            if (objectHit.hit) {
                selection.clearLights();

                if (toggling && selection.hasObject(objectHit.object)) {
                    selection.deselectObject(objectHit.object);
                } else {
                    selection.selectObject(objectHit.object);
                    selection.setActiveObject(objectHit.object);
                }
            }
        } else {
            const ObjectHandle active = selection.getActiveObject();
            const u32 mode = ictx.getSelectionContext();

            // What the click hits on the active object, in the current mode
            const VertexHit vertexHit = (mode & InputContext_SelectionVertex) ? pickVertex(ctx.scene, ray, 0.03f, active) : VertexHit {};
            const EdgeHit edgeHit = (mode & InputContext_SelectionEdge) ? pickEdge(ctx.scene, ray, 0.03f, active) : EdgeHit {};
            const FaceHit faceHit = pickFace(ctx.scene, ray, active, INVALID_OBJECT, culledFaces(ctx));

            f32 activeDistance = faceHit.hit ? faceHit.distance : FLT_MAX;
            if (vertexHit.hit) activeDistance = std::min(activeDistance, vertexHit.distance);
            if (edgeHit.hit) activeDistance = std::min(activeDistance, edgeHit.distance);

            // Another object in front of the active one becomes the active object, in the same mode
            const FaceHit other = uvWorkspace ? FaceHit {} : pickFace(ctx.scene, ray, INVALID_OBJECT, active, culledFaces(ctx));

            if (other.hit && other.distance < activeDistance) {
                selection.clearLights();
                selection.setActiveObject(other.object);
            } else if (mode & InputContext_SelectionVertex) {
                if (vertexHit.hit) {
                    selection.clearLights();

                    if (toggling && selection.hasVertex(vertexHit.object, vertexHit.vertex)) {
                        selection.removeVertex(vertexHit.object, vertexHit.vertex);
                    } else {
                        selection.addVertex(vertexHit.object, vertexHit.vertex);
                    }
                }
            } else if (mode & InputContext_SelectionEdge) {
                if (edgeHit.hit) {
                    selection.clearLights();

                    const MeshData& mesh = ctx.scene.objects.get(edgeHit.object).meshData;

                    if (toggling && isEdgeSelected(selection, edgeHit.object, mesh, edgeHit.edge)) {
                        deselectEdge(selection, edgeHit.object, mesh, edgeHit.edge);
                    } else {
                        selectEdge(selection, edgeHit.object, mesh, edgeHit.edge);
                    }
                }
            } else if (mode & InputContext_SelectionFace) {
                if (faceHit.hit) {
                    selection.clearLights();

                    const MeshData& mesh = ctx.scene.objects.get(faceHit.object).meshData;

                    // Island mode takes every face connected to it on the texture
                    const std::vector<FaceHandle> faces = ctx.workspace.uvIslands ? mesh.getUVIsland(faceHit.face) : std::vector<FaceHandle> { faceHit.face };
                    const bool deselecting = toggling && selection.hasFace(faceHit.object, faceHit.face);
                    for (FaceHandle face : faces) {
                        if (deselecting) deselectFace(selection, faceHit.object, mesh, face);
                        else selectFace(selection, faceHit.object, mesh, face);
                    }
                }
            }
        }
    }
}
