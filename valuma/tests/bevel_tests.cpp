#include "test.hpp"
#include "mesh_helpers.hpp"

#include <algorithm>
#include <cmath>

namespace {
    MeshData flat(PresetMesh preset) {
        MeshData mesh;
        mesh.setMesh(preset);
        return mesh;
    }

    VertexHandle vertexAt(const MeshData& mesh, Vec3 position) {
        for (VertexHandle vertex : mesh.getVertexHandles()) {
            if ((mesh.getVertexPosition(vertex) - position).length() < 1e-4f) return vertex;
        }
        return INVALID_VERTEX;
    }

    bool hasVertexAt(const MeshData& mesh, Vec3 position) {
        return mesh.isValidHandle(vertexAt(mesh, position));
    }

    // Every vertex on the grid's open edge still lies on its square outline
    bool borderVerticesOnOutline(const MeshData& mesh) {
        for (VertexHandle vertex : mesh.getVertexHandles()) {
            if (!mesh.isBorderVertex(vertex)) continue;
            const Vec3 p = mesh.getVertexPosition(vertex);
            const bool onOutline = std::fabs(std::fabs(p.x) - 1.0f) < 1e-4f || std::fabs(std::fabs(p.z) - 1.0f) < 1e-4f;
            if (!onOutline || std::fabs(p.y) > 1e-5f) return false;
        }
        return true;
    }

    // Separate holes in the mesh: a stray one means the bevel left a gap
    u32 borderLoopCount(const MeshData& mesh) {
        std::vector<EdgeHandle> seen;
        u32 loops = 0;

        for (EdgeHandle edge : mesh.getEdgeHandles()) {
            if (!mesh.isBorder(edge) || std::find(seen.begin(), seen.end(), edge) != seen.end()) continue;

            ++loops;
            for (EdgeHandle step : mesh.getLoopEdges(edge)) seen.push_back(step);
        }
        return loops;
    }

    VertexHandle findVertexWithEdges(const MeshData& mesh, size_t edgeCount) {
        for (VertexHandle vertex : mesh.getVertexHandles()) {
            if (mesh.getOutgoingEdges(vertex).size() == edgeCount) return vertex;
        }
        return INVALID_VERTEX;
    }
}

TEST_CASE(bevel_vertex) {
    MeshData mesh = cube();
    VertexHandle corner = mesh.getVertexHandles()[0];
    const Vec3 position = mesh.getVertexPosition(corner);
    SlideSession session;

    CHECK(mesh.bevelVertex(corner, session));
    CHECK(mesh.validate());
    CHECK(faceSides(mesh) == "3444555");

    mesh.setSlideWidth(session, 0.25f);

    for (VertexHandle vertex : session.vertices) {
        CHECK(std::fabs((mesh.getVertexPosition(vertex) - position).length() - 0.25f) < 1e-4f);
    }

    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_edge) {
    MeshData mesh = cube();
    SlideSession session;

    CHECK(mesh.bevelEdge(mesh.getEdgeHandles()[0], session));
    CHECK(mesh.validate());
    CHECK(faceSides(mesh) == "4444455");
    CHECK(std::fabs(session.maxWidth - 1.0f) < 1e-4f);

    mesh.setSlideWidth(session, 0.2f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_face) {
    MeshData mesh = cube();
    SlideSession session;

    CHECK(mesh.bevelFace(mesh.getFaceHandles()[0], session));
    CHECK(mesh.validate());
    CHECK(faceSides(mesh) == "4444444444");
    CHECK(std::fabs(session.maxWidth - 0.5f) < 1e-4f);

    mesh.setSlideWidth(session, 0.2f);
    CHECK(everyFaceTriangulates(mesh));

    // Inset corners sit 0.2 from both edges (0.2 * sqrt 2 from the corner); sliding ones 0.2 along their edge.
    int inset = 0;
    int sliding = 0;

    for (const Vec3& direction : session.directions) {
        const f32 moved = (direction * 0.2f).length();
        if (std::fabs(moved - 0.2f * std::sqrt(2.0f)) < 1e-4f) ++inset;
        else if (std::fabs(moved - 0.2f) < 1e-4f) ++sliding;
    }

    CHECK(inset == 4);
    CHECK(sliding == 4);
}

TEST_CASE(bevel_width_clamps_to_max) {
    MeshData mesh = cube();
    SlideSession session;
    mesh.bevelEdge(mesh.getEdgeHandles()[0], session);

    mesh.setSlideWidth(session, 5.0f);

    for (size_t i = 0; i < session.vertices.size(); ++i) {
        const Vec3 expected = session.starts[i] + session.directions[i] * session.maxWidth;
        CHECK((mesh.getVertexPosition(session.vertices[i]) - expected).length() < 1e-4f);
    }
}

TEST_CASE(bevel_cancel_restores_mesh) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    SlideSession session;

    mesh.bevelFace(face, session);
    mesh.setSlideWidth(session, 0.3f);
    mesh.cancelSlide(session);

    CHECK(mesh.validate());
    CHECK(counts(mesh, 8, 24, 6));
    CHECK(mesh.isValidHandle(face));
}

TEST_CASE(bevel_edge_between_four_edge_corners) {
    MeshData mesh = cube();
    extrude(mesh, mesh.getFaceHandles()[0]);

    VertexHandle corner = findVertexWithEdges(mesh, 4);
    EdgeHandle edge = INVALID_EDGE;
    for (EdgeHandle spoke : mesh.getOutgoingEdges(corner)) {
        if (mesh.getOutgoingEdges(mesh.getEdgeTip(spoke)).size() == 4) { edge = spoke; break; }
    }

    SlideSession session;
    CHECK(mesh.bevelEdge(edge, session));
    CHECK(mesh.validate());

    mesh.setSlideWidth(session, 0.1f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_extruded_top) {
    MeshData mesh = cube();
    FaceHandle top = mesh.getFaceHandles()[0];
    extrude(mesh, top);

    SlideSession session;
    CHECK(mesh.bevelFace(top, session));
    CHECK(mesh.validate());

    mesh.setSlideWidth(session, 0.1f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_extrusion_side) {
    MeshData mesh = cube();
    extrude(mesh, mesh.getFaceHandles()[0]);

    FaceHandle side = INVALID_FACE;
    for (FaceHandle face : mesh.getFaceHandles()) {
        int fourEdgeCorners = 0;
        for (VertexHandle corner : mesh.getFaceVertices(face)) {
            if (mesh.getOutgoingEdges(corner).size() == 4) ++fourEdgeCorners;
        }
        if (fourEdgeCorners == 2) { side = face; break; }
    }

    SlideSession session;
    CHECK(mesh.bevelFace(side, session));
    CHECK(mesh.validate());

    mesh.setSlideWidth(session, 0.1f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_four_edge_vertex) {
    MeshData mesh = cube();
    extrude(mesh, mesh.getFaceHandles()[0]);

    SlideSession session;
    CHECK(mesh.bevelVertex(findVertexWithEdges(mesh, 4), session));
    CHECK(mesh.validate());

    mesh.setSlideWidth(session, 0.1f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_border_edge_of_an_open_cube) {
    // The edge now borders a hole: the bevel makes a strip on the face side and leaves the hole's outline alone
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    EdgeHandle edge = mesh.getFace(face)->edge;
    const Vec3 origin = mesh.getVertexPosition(mesh.getEdgeOrigin(edge));
    const Vec3 tip = mesh.getVertexPosition(mesh.getEdgeTip(edge));
    mesh.removeFace(face);

    SlideSession session;
    CHECK(mesh.bevelEdge(edge, session));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == 6);

    mesh.setSlideWidth(session, 0.3f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
    CHECK(hasVertexAt(mesh, origin));
    CHECK(hasVertexAt(mesh, tip));
    CHECK(borderLoopCount(mesh) == 1);

    mesh.cancelSlide(session);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 8, 24, 5));
}

TEST_CASE(bevel_plane_corner_cuts_it_off) {
    // A corner with two border edges and one face: the face gets a chamfer, no cap
    MeshData mesh = flat(PresetMesh::Plane);
    const VertexHandle corner = mesh.getVertexHandles()[0];
    const Vec3 position = mesh.getVertexPosition(corner);

    SlideSession session;
    CHECK(mesh.bevelVertex(corner, session));
    CHECK(mesh.validate());
    CHECK(counts(mesh, 5, 10, 1));

    mesh.setSlideWidth(session, 0.25f);
    for (VertexHandle vertex : session.vertices) {
        CHECK(std::fabs((mesh.getVertexPosition(vertex) - position).length() - 0.25f) < 1e-4f);
    }
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
    CHECK(borderLoopCount(mesh) == 1);
}

TEST_CASE(bevel_grid_border_vertex) {
    // Three edges, two of them border: the cut gets a triangle cap whose third side is new border
    MeshData mesh = flat(PresetMesh::Grid);
    SlideSession session;

    CHECK(mesh.bevelVertex(vertexAt(mesh, Vec3(-1.0f, 0.0f, 0.0f)), session));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == 101);

    mesh.setSlideWidth(session, 0.1f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
    CHECK(borderVerticesOnOutline(mesh));
    CHECK(borderLoopCount(mesh) == 1);
}

TEST_CASE(bevel_edge_running_into_the_border) {
    // An interior edge with one end on the border: the strip slides along the border to reach it
    MeshData mesh = flat(PresetMesh::Grid);
    const EdgeHandle edge = findEdgeBetween(mesh, vertexAt(mesh, Vec3(-1.0f, 0.0f, 0.0f)), vertexAt(mesh, Vec3(-0.8f, 0.0f, 0.0f)));
    SlideSession session;

    CHECK(mesh.bevelEdge(edge, session));
    CHECK(mesh.validate());

    mesh.setSlideWidth(session, 0.05f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
    CHECK(borderVerticesOnOutline(mesh));
    CHECK(borderLoopCount(mesh) == 1);
}

TEST_CASE(bevel_border_edge_makes_a_one_sided_strip) {
    // The grid's outline stays put: the edge's ends are kept and a new edge runs parallel inside
    MeshData mesh = flat(PresetMesh::Grid);
    const VertexHandle a = vertexAt(mesh, Vec3(-1.0f, 0.0f, 0.0f));
    const VertexHandle b = vertexAt(mesh, Vec3(-1.0f, 0.0f, 0.2f));
    EdgeHandle edge = findEdgeBetween(mesh, a, b);
    if (!mesh.isValidHandle(edge)) edge = findEdgeBetween(mesh, b, a);
    SlideSession session;

    CHECK(mesh.bevelEdge(edge, session));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == 101);

    mesh.setSlideWidth(session, 0.1f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
    CHECK(mesh.isValidHandle(a) && mesh.isValidHandle(b));
    CHECK(borderVerticesOnOutline(mesh));
    CHECK(borderLoopCount(mesh) == 1);
}

TEST_CASE(bevel_face_in_a_grid_corner) {
    MeshData mesh = flat(PresetMesh::Grid);
    FaceHandle cornerCell = INVALID_FACE;
    for (FaceHandle face : mesh.getFaceHandles()) {
        for (VertexHandle vertex : mesh.getFaceVertices(face)) {
            if ((mesh.getVertexPosition(vertex) - Vec3(-1.0f, 0.0f, -1.0f)).length() < 1e-4f) cornerCell = face;
        }
    }

    SlideSession session;
    CHECK(mesh.bevelFace(cornerCell, session));
    CHECK(mesh.validate());

    mesh.setSlideWidth(session, 0.05f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
    CHECK(borderVerticesOnOutline(mesh));
    CHECK(borderLoopCount(mesh) == 1);
}

TEST_CASE(bevel_vertex_on_the_rim_of_a_hole) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    const VertexHandle corner = mesh.getEdgeOrigin(mesh.getFace(face)->edge);
    mesh.removeFace(face);

    SlideSession session;
    CHECK(mesh.bevelVertex(corner, session));
    CHECK(mesh.validate());

    mesh.setSlideWidth(session, 0.3f);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
    CHECK(borderLoopCount(mesh) == 1);
}

TEST_CASE(bevel_again_on_beveled_mesh) {
    MeshData mesh = cube();
    SlideSession session;
    mesh.bevelFace(mesh.getFaceHandles()[0], session);
    mesh.setSlideWidth(session, 0.2f);

    int beveled = 0;

    for (int round = 0; round < 3; ++round) {
        for (EdgeHandle edge : mesh.getEdgeHandles()) {
            SlideSession next;
            if (mesh.bevelEdge(edge, next)) {
                mesh.setSlideWidth(next, next.maxWidth * 0.3f);
                ++beveled;
                break;
            }
        }
    }

    CHECK(beveled == 3);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_measured_in_a_scaled_space) {
    // A cube squashed to half height: the bevel is even in that space, not in mesh space
    const Mat4 squash = Mat4::scale(Vec3(1.0f, 0.5f, 1.0f));

    MeshData plain = cube();
    SlideSession plainSession;
    CHECK(plain.bevelVertex(plain.getVertexHandles()[0], plainSession));

    MeshData mesh = cube();
    VertexHandle corner = mesh.getVertexHandles()[0];
    const Vec3 position = mesh.getVertexPosition(corner);
    SlideSession session;

    CHECK(mesh.bevelVertex(corner, session, squash));
    CHECK(mesh.validate());

    // The shorter vertical edge now limits the width
    CHECK(std::fabs(session.maxWidth - plainSession.maxWidth * 0.5f) < 1e-4f);

    mesh.setSlideWidth(session, 0.25f);

    for (VertexHandle vertex : session.vertices) {
        const Vec3 offset = mesh.getVertexPosition(vertex) - position;
        const Vec3 squashed(offset.x, offset.y * 0.5f, offset.z);
        CHECK(std::fabs(squashed.length() - 0.25f) < 1e-4f);
    }

    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_in_a_space_keeps_other_vertices_exact) {
    // Going through the space and back must not nudge vertices the bevel didn't create
    MeshData mesh = cube();
    const MeshData before = mesh;
    SlideSession session;

    Mat4 space = Mat4::rotationY(0.7f) * Mat4::scale(Vec3(1.3f, 0.6f, 2.1f));
    CHECK(mesh.bevelVertex(mesh.getVertexHandles()[0], session, space));

    for (VertexHandle vertex : before.getVertexHandles()) {
        if (!mesh.isValidHandle(vertex)) continue;
        const Vec3 a = before.getVertexPosition(vertex);
        const Vec3 b = mesh.getVertexPosition(vertex);
        CHECK(a.x == b.x && a.y == b.y && a.z == b.z);
    }

    mesh.cancelSlide(session);
    CHECK(mesh.getVertexHandles().size() == before.getVertexHandles().size());
    for (VertexHandle vertex : before.getVertexHandles()) {
        const Vec3 a = before.getVertexPosition(vertex);
        const Vec3 b = mesh.getVertexPosition(vertex);
        CHECK(a.x == b.x && a.y == b.y && a.z == b.z);
    }
}
