#include "application/uv/uv_operations.hpp"
#include "scene/mesh/uv_unwrap.hpp"

#include <string>

namespace {
    Object* uvTarget(AppContext& ctx) {
        return ctx.scene.objects.tryGet(uvObject(ctx));
    }

    f32 margin(const AppContext& ctx) {
        return ctx.workspace.uvMargin / 100.0f;
    }

    std::string count(std::size_t n, const char* one, const char* many) {
        return std::to_string(n) + " " + (n == 1 ? one : many);
    }

    // After new UVs: every face packs into the whole texture; some faces fit back where they were
    void place(AppContext& ctx, MeshData& mesh, const std::vector<std::vector<FaceHandle>>& islands, bool everything,
               const UVUnwrap::Area& before, bool rotate) {
        UVUnwrap::Area area;
        if (!everything && before.high.x - before.low.x > 1e-6f && before.high.y - before.low.y > 1e-6f) area = before;
        UVUnwrap::pack(mesh, islands, area, margin(ctx), rotate);
    }
}

bool hasUVObject(const AppContext& ctx) {
    return ctx.scene.objects.isValid(uvObject(ctx));
}

std::vector<FaceHandle> uvTargetFaces(const AppContext& ctx) {
    const ObjectHandle handle = uvObject(ctx);
    const Object* object = ctx.scene.objects.tryGet(handle);
    if (!object) return {};
    const MeshData& mesh = object->meshData;
    const Selection& selection = ctx.scene.selection;

    std::vector<FaceHandle> faces;
    if (ctx.systems.input_ctx.getModeContext() == InputContext_SelectionFace) {
        faces = selection.getFaceHandles();
    } else if (selection.hasVertices()) {
        for (FaceHandle face : mesh.getFaceHandles()) {
            bool all = true;
            for (VertexHandle vertex : mesh.getFaceVertices(face)) all = all && selection.hasVertex(handle, vertex);
            if (all) faces.push_back(face);
        }
    }
    return faces.empty() ? mesh.getFaceHandles() : faces;
}

bool canMarkSeams(const AppContext& ctx) {
    return hasUVObject(ctx) && ctx.systems.input_ctx.getModeContext() == InputContext_SelectionEdge && ctx.scene.selection.hasEdges();
}

void markSeams(AppContext& ctx, bool seam) {
    Object* object = uvTarget(ctx);
    if (!object || !canMarkSeams(ctx)) return;

    ctx.history.begin(ctx.scene);
    const std::vector<EdgeHandle> edges = ctx.scene.selection.getEdgeHandles();
    for (EdgeHandle edge : edges) object->meshData.setSeam(edge, seam);
    ctx.history.commit();
    ctx.systems.console.print(count(edges.size(), "edge", "edges") + (seam ? " marked as seams" : " no longer seams"));
}

void seamsFromIslands(AppContext& ctx) {
    Object* object = uvTarget(ctx);
    if (!object) return;

    ctx.history.begin(ctx.scene);
    const u32 marked = object->meshData.markSeamsFromIslands();
    ctx.history.commit();
    ctx.systems.console.print(count(marked, "seam", "seams") + " marked where the layout is cut");
}

void unwrapUVs(AppContext& ctx) {
    Object* object = uvTarget(ctx);
    if (!object) return;
    MeshData& mesh = object->meshData;
    const std::vector<FaceHandle> faces = uvTargetFaces(ctx);
    const bool everything = faces.size() == mesh.getFaceHandles().size();

    UVUnwrap::Area before;
    UVUnwrap::bounds(mesh, faces, before);

    ctx.history.begin(ctx.scene);
    const auto islands = UVUnwrap::unwrap(mesh, faces, ctx.scene.objects.worldMatrix(uvObject(ctx)));
    place(ctx, mesh, islands, everything, before, true);
    object->meshDirty = true;
    ctx.history.commit();
    ctx.systems.console.print("Unwrapped " + count(faces.size(), "face", "faces") + " into " + count(islands.size(), "island", "islands"));
}

void projectUVs(AppContext& ctx, UVProjection projection) {
    Object* object = uvTarget(ctx);
    if (!object) return;
    MeshData& mesh = object->meshData;
    const std::vector<FaceHandle> faces = uvTargetFaces(ctx);
    const bool everything = faces.size() == mesh.getFaceHandles().size();
    const Mat4 world = ctx.scene.objects.worldMatrix(uvObject(ctx));

    UVUnwrap::Area before;
    UVUnwrap::bounds(mesh, faces, before);

    ctx.history.begin(ctx.scene);
    switch (projection) {
        case UVProjection::View: {
            // As the camera sees them: the view's right is u, its up runs against v
            const Camera& camera = ctx.scene.camera;
            const Vec3 right = camera.getRight().normalized();
            const Vec3 up = Vec3::cross(right, camera.getForward().normalized());
            UVUnwrap::projectPlanar(mesh, faces, world, right, up);
            break;
        }
        case UVProjection::Box: UVUnwrap::projectBox(mesh, faces, world); break;
        case UVProjection::Cylinder: UVUnwrap::projectCylinder(mesh, faces); break;
        case UVProjection::Sphere: UVUnwrap::projectSphere(mesh, faces); break;
    }
    // Projections stay upright, as projected
    const auto islands = UVUnwrap::uvIslands(mesh, faces);
    place(ctx, mesh, islands, everything, before, false);
    object->meshDirty = true;
    ctx.history.commit();

    const char* names[] = { "from the view", "as a box", "as a cylinder", "as a sphere" };
    ctx.systems.console.print("Projected " + count(faces.size(), "face", "faces") + " " + names[static_cast<u32>(projection)]);
}

void packUVs(AppContext& ctx) {
    Object* object = uvTarget(ctx);
    if (!object) return;
    MeshData& mesh = object->meshData;

    ctx.history.begin(ctx.scene);
    const auto islands = UVUnwrap::uvIslands(mesh, mesh.getFaceHandles());
    UVUnwrap::pack(mesh, islands, UVUnwrap::Area {}, margin(ctx), true);
    object->meshDirty = true;
    ctx.history.commit();
    ctx.systems.console.print("Packed " + count(islands.size(), "island", "islands"));
}
