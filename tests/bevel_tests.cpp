#include "test.hpp"
#include "mesh_helpers.hpp"

#include <cmath>

namespace {
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
    BevelSession session;

    CHECK(mesh.bevelVertex(corner, session));
    CHECK(mesh.validate());
    CHECK(faceSides(mesh) == "3444555");

    mesh.setBevelWidth(session, 0.25f);

    for (VertexHandle vertex : session.vertices) {
        CHECK(std::fabs((mesh.getVertexPosition(vertex) - position).length() - 0.25f) < 1e-4f);
    }

    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_edge) {
    MeshData mesh = cube();
    BevelSession session;

    CHECK(mesh.bevelEdge(mesh.getEdgeHandles()[0], session));
    CHECK(mesh.validate());
    CHECK(faceSides(mesh) == "4444455");
    CHECK(std::fabs(session.maxWidth - 1.0f) < 1e-4f);

    mesh.setBevelWidth(session, 0.2f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_face) {
    MeshData mesh = cube();
    BevelSession session;

    CHECK(mesh.bevelFace(mesh.getFaceHandles()[0], session));
    CHECK(mesh.validate());
    CHECK(faceSides(mesh) == "4444444444");
    CHECK(std::fabs(session.maxWidth - 0.5f) < 1e-4f);

    mesh.setBevelWidth(session, 0.2f);
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
    BevelSession session;
    mesh.bevelEdge(mesh.getEdgeHandles()[0], session);

    mesh.setBevelWidth(session, 5.0f);

    for (size_t i = 0; i < session.vertices.size(); ++i) {
        const Vec3 expected = session.starts[i] + session.directions[i] * session.maxWidth;
        CHECK((mesh.getVertexPosition(session.vertices[i]) - expected).length() < 1e-4f);
    }
}

TEST_CASE(bevel_cancel_restores_mesh) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    BevelSession session;

    mesh.bevelFace(face, session);
    mesh.setBevelWidth(session, 0.3f);
    mesh.cancelBevel(session);

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

    BevelSession session;
    CHECK(mesh.bevelEdge(edge, session));
    CHECK(mesh.validate());

    mesh.setBevelWidth(session, 0.1f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_extruded_top) {
    MeshData mesh = cube();
    FaceHandle top = mesh.getFaceHandles()[0];
    extrude(mesh, top);

    BevelSession session;
    CHECK(mesh.bevelFace(top, session));
    CHECK(mesh.validate());

    mesh.setBevelWidth(session, 0.1f);
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

    BevelSession session;
    CHECK(mesh.bevelFace(side, session));
    CHECK(mesh.validate());

    mesh.setBevelWidth(session, 0.1f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_four_edge_vertex) {
    MeshData mesh = cube();
    extrude(mesh, mesh.getFaceHandles()[0]);

    BevelSession session;
    CHECK(mesh.bevelVertex(findVertexWithEdges(mesh, 4), session));
    CHECK(mesh.validate());

    mesh.setBevelWidth(session, 0.1f);
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(bevel_refused_next_to_border) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    EdgeHandle edge = mesh.getFace(face)->edge;
    mesh.removeFace(face);

    BevelSession session;
    CHECK(!mesh.bevelEdge(edge, session));
    CHECK(mesh.validate());
    CHECK(counts(mesh, 8, 24, 5));
}

TEST_CASE(bevel_again_on_beveled_mesh) {
    MeshData mesh = cube();
    BevelSession session;
    mesh.bevelFace(mesh.getFaceHandles()[0], session);
    mesh.setBevelWidth(session, 0.2f);

    int beveled = 0;

    for (int round = 0; round < 3; ++round) {
        for (EdgeHandle edge : mesh.getEdgeHandles()) {
            BevelSession next;
            if (mesh.bevelEdge(edge, next)) {
                mesh.setBevelWidth(next, next.maxWidth * 0.3f);
                ++beveled;
                break;
            }
        }
    }

    CHECK(beveled == 3);
    CHECK(mesh.validate());
    CHECK(everyFaceTriangulates(mesh));
}
