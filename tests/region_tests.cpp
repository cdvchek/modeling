#include "test.hpp"
#include "mesh_helpers.hpp"

#include <cmath>

namespace {
    MeshData grid() {
        MeshData mesh;
        mesh.setMesh(PresetMesh::Grid);
        return mesh;
    }

    Vec3 faceCenter(const MeshData& mesh, FaceHandle face) {
        Vec3 center(0.0f);
        const std::vector<VertexHandle> corners = mesh.getFaceVertices(face);
        for (VertexHandle corner : corners) center += mesh.getVertexPosition(corner);
        return center / static_cast<f32>(corners.size());
    }

    // The grid cell at column, row (0..9), counted from the -x, -z corner
    FaceHandle cell(const MeshData& mesh, u32 column, u32 row) {
        const Vec3 target(-0.9f + 0.2f * static_cast<f32>(column), 0.0f, -0.9f + 0.2f * static_cast<f32>(row));
        FaceHandle best = INVALID_FACE;
        f32 bestDistance = 1e9f;

        for (FaceHandle face : mesh.getFaceHandles()) {
            const f32 distance = (faceCenter(mesh, face) - target).length();
            if (distance < bestDistance) {
                bestDistance = distance;
                best = face;
            }
        }
        return best;
    }

    // Cube faces on opposite sides share no vertex
    std::vector<FaceHandle> oppositeCubeFaces(const MeshData& mesh) {
        const std::vector<FaceHandle> faces = mesh.getFaceHandles();
        const Vec3 normal = mesh.getFaceNormal(faces[0]).normalized();

        for (FaceHandle face : faces) {
            if (Vec3::dot(mesh.getFaceNormal(face).normalized(), normal) < -0.9f) return { faces[0], face };
        }
        return {};
    }
}

TEST_CASE(extrude_one_face_region) {
    MeshData mesh = cube();
    std::vector<FaceHandle> tops;

    CHECK(mesh.extrudeRegions({ mesh.getFaceHandles()[0] }, tops) == RegionError::None);
    CHECK(mesh.validate());
    CHECK(tops.size() == 1);
    CHECK(counts(mesh, 12, 2 * 20, 10));
}

TEST_CASE(extrude_adjacent_faces_as_one_region) {
    // Two grid cells side by side: one block, walls only around the outside (6 boundary edges)
    MeshData mesh = grid();
    std::vector<FaceHandle> tops;

    CHECK(mesh.extrudeRegions({ cell(mesh, 4, 4), cell(mesh, 5, 4) }, tops) == RegionError::None);
    CHECK(mesh.validate());
    CHECK(tops.size() == 2);
    CHECK(mesh.getFaceHandles().size() == 100 + 6);
    CHECK(mesh.getVertexHandles().size() == 121 + 6);

    // Walls have no height until the grab moves the top; lift it, the way the grab would
    std::vector<VertexHandle> moved;
    for (FaceHandle top : tops) {
        for (VertexHandle vertex : mesh.getFaceVertices(top)) {
            if (std::find(moved.begin(), moved.end(), vertex) == moved.end()) moved.push_back(vertex);
        }
    }
    for (VertexHandle vertex : moved) mesh.translateVertex(vertex, Vec3(0.0f, 0.3f, 0.0f));

    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(extrude_separate_regions_together) {
    MeshData mesh = cube();
    std::vector<FaceHandle> tops;

    CHECK(mesh.extrudeRegions(oppositeCubeFaces(mesh), tops) == RegionError::None);
    CHECK(mesh.validate());
    CHECK(tops.size() == 2);
    CHECK(counts(mesh, 16, 2 * 28, 14));
}

TEST_CASE(extrude_region_on_the_mesh_border) {
    // A corner cell of the grid: two of its boundary edges are open border
    MeshData mesh = grid();
    std::vector<FaceHandle> tops;

    CHECK(mesh.extrudeRegions({ cell(mesh, 0, 0) }, tops) == RegionError::None);
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == 100 + 4);
}

TEST_CASE(region_refusals_leave_the_mesh_alone) {
    MeshData mesh = grid();
    std::vector<FaceHandle> tops;

    // Diagonal neighbors touch only at a corner
    CHECK(mesh.extrudeRegions({ cell(mesh, 2, 2), cell(mesh, 3, 3) }, tops) == RegionError::CornerTouch);

    // A ring of 8 cells around an unselected one has two boundary loops
    std::vector<FaceHandle> ring;
    for (u32 column = 4; column <= 6; ++column) {
        for (u32 row = 4; row <= 6; ++row) {
            if (column != 5 || row != 5) ring.push_back(cell(mesh, column, row));
        }
    }
    CHECK(mesh.extrudeRegions(ring, tops) == RegionError::Holes);

    CHECK(mesh.extrudeRegions({}, tops) == RegionError::NoFaces);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 121, 2 * (2 * 10 * 11), 100));

    // Every face of a closed cube: nothing to build walls from
    MeshData closed = cube();
    CHECK(closed.extrudeRegions(closed.getFaceHandles(), tops) == RegionError::NoBoundary);
    CHECK(counts(closed, 8, 24, 6));

    // Insets refuse the same way, and leave positions untouched
    SlideSession session;
    std::vector<FaceHandle> inner;
    const Vec3 before = mesh.getVertexPosition(mesh.getVertexHandles()[0]);
    CHECK(mesh.insetRegions({ cell(mesh, 2, 2), cell(mesh, 3, 3) }, session, inner) == RegionError::CornerTouch);
    const Vec3 after = mesh.getVertexPosition(mesh.getVertexHandles()[0]);
    CHECK(before.x == after.x && before.y == after.y && before.z == after.z);
    CHECK(counts(mesh, 121, 2 * (2 * 10 * 11), 100));
}

TEST_CASE(inset_one_face_by_a_width) {
    // A grid cell is 0.2 wide: insetting by 0.05 leaves a 0.1 square, each side 0.05 in from the old one
    MeshData mesh = grid();
    const FaceHandle face = cell(mesh, 5, 5);
    const Vec3 center = faceCenter(mesh, face);
    SlideSession session;
    std::vector<FaceHandle> inner;

    CHECK(mesh.insetRegions({ face }, session, inner) == RegionError::None);
    CHECK(mesh.validate());
    CHECK(inner.size() == 1);
    CHECK(std::fabs(session.maxWidth - 0.1f) < 1e-4f);

    mesh.setSlideWidth(session, 0.05f);
    for (VertexHandle vertex : mesh.getFaceVertices(inner[0])) {
        const Vec3 offset = mesh.getVertexPosition(vertex) - center;
        CHECK(std::fabs(std::fabs(offset.x) - 0.05f) < 1e-4f);
        CHECK(std::fabs(std::fabs(offset.z) - 0.05f) < 1e-4f);
        CHECK(std::fabs(offset.y) < 1e-5f);
    }
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(inset_a_region_borders_only_its_outside) {
    // Two cells side by side get one border ring of 6 quads; the edge between them stays inside
    MeshData mesh = grid();
    SlideSession session;
    std::vector<FaceHandle> inner;

    CHECK(mesh.insetRegions({ cell(mesh, 4, 4), cell(mesh, 5, 4) }, session, inner) == RegionError::None);
    CHECK(mesh.getFaceHandles().size() == 100 + 6);

    mesh.setSlideWidth(session, 0.05f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));

    // Cancel puts the mesh back exactly
    mesh.cancelSlide(session);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 121, 2 * (2 * 10 * 11), 100));
}

TEST_CASE(inset_across_a_fold) {
    // Two cube faces meeting at an edge: the border follows each face's own plane
    MeshData mesh = cube();
    const std::vector<FaceHandle> faces = mesh.getFaceHandles();
    std::vector<FaceHandle> pair { faces[0] };

    for (FaceHandle face : faces) {
        if (face == faces[0]) continue;
        if (std::fabs(Vec3::dot(mesh.getFaceNormal(face).normalized(), mesh.getFaceNormal(faces[0]).normalized())) < 0.1f) {
            pair.push_back(face);
            break;
        }
    }

    SlideSession session;
    std::vector<FaceHandle> inner;
    CHECK(mesh.insetRegions(pair, session, inner) == RegionError::None);
    CHECK(session.maxWidth > 0.0f);

    mesh.setSlideWidth(session, 0.2f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
}
