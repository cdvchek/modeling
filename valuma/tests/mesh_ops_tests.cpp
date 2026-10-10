#include "test.hpp"
#include "mesh_helpers.hpp"

TEST_CASE(cube_is_valid) {
    MeshData mesh = cube();
    CHECK(mesh.validate());
    CHECK(counts(mesh, 8, 24, 6));
}

TEST_CASE(extrude_twice) {
    MeshData mesh = cube();
    FaceHandle top = mesh.insertFaceRing(mesh.getFaceHandles()[0]);
    CHECK(mesh.isValidHandle(top));
    CHECK(mesh.validate());

    top = mesh.insertFaceRing(top);
    CHECK(mesh.isValidHandle(top));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceVertices(top).size() == 4);
}

TEST_CASE(extrude_next_to_border) {
    MeshData mesh = cube();
    mesh.removeFace(mesh.getFaceHandles()[0]);
    CHECK(mesh.isValidHandle(mesh.insertFaceRing(mesh.getFaceHandles()[1])));
    CHECK(mesh.validate());
}

TEST_CASE(extrude_five_sided_face) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    mesh.splitEdge(mesh.getFace(face)->edge);

    CHECK(mesh.isValidHandle(mesh.insertFaceRing(face)));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceVertices(face).size() == 5);
}

TEST_CASE(remove_and_fill_face) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    EdgeHandle edge = mesh.getFace(face)->edge;

    CHECK(mesh.removeFace(face));
    CHECK(mesh.validate());
    CHECK(mesh.isBorder(edge));

    CHECK(mesh.fillFaceLoop(edge));
    CHECK(mesh.validate());
}

TEST_CASE(remove_edge_and_fill_hole) {
    MeshData mesh = cube();
    CHECK(mesh.removeEdge(mesh.getEdgeHandles()[0]));
    CHECK(mesh.validate());

    EdgeHandle border = INVALID_EDGE;
    for (EdgeHandle edge : mesh.getEdgeHandles()) {
        if (mesh.isBorder(edge)) { border = edge; break; }
    }

    CHECK(mesh.getLoopEdges(border).size() == 6);
    CHECK(mesh.fillFaceLoop(border));
    CHECK(mesh.validate());
}

TEST_CASE(remove_vertex) {
    MeshData mesh = cube();
    CHECK(mesh.removeVertex(mesh.getVertexHandles()[0]));
    CHECK(mesh.validate());
}

TEST_CASE(remove_wire_edges_down_to_nothing) {
    MeshData mesh = cube();
    for (FaceHandle face : mesh.getFaceHandles()) mesh.removeFace(face);
    CHECK(mesh.validate());

    while (!mesh.getEdgeHandles().empty()) {
        if (!mesh.removeEdge(mesh.getEdgeHandles()[0]) || !mesh.validate()) break;
    }

    CHECK(mesh.getEdgeHandles().empty());
    CHECK(mesh.validate());
}

TEST_CASE(split_edge) {
    MeshData mesh = cube();
    CHECK(mesh.isValidHandle(mesh.splitEdge(mesh.getEdgeHandles()[0])));
    CHECK(mesh.validate());
}

TEST_CASE(split_wire_edge) {
    MeshData mesh = cube();
    for (FaceHandle face : mesh.getFaceHandles()) mesh.removeFace(face);

    CHECK(mesh.isValidHandle(mesh.splitEdge(mesh.getEdgeHandles()[0])));
    CHECK(mesh.validate());
}

TEST_CASE(connect_diagonal) {
    MeshData mesh = cube();
    auto corners = mesh.getFaceVertices(mesh.getFaceHandles()[0]);

    CHECK(mesh.connectVertices(corners[0], corners[2]));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == 7);
}

TEST_CASE(merge_across_quads) {
    MeshData mesh = cube();
    auto corners = mesh.getFaceVertices(mesh.getFaceHandles()[0]);

    CHECK(mesh.mergeVertices(corners[0], corners[1], 0));
    CHECK(mesh.validate());
}

TEST_CASE(merge_with_triangle_on_one_side) {
    MeshData mesh = cube();
    auto corners = mesh.getFaceVertices(mesh.getFaceHandles()[0]);
    mesh.connectVertices(corners[0], corners[2]);

    CHECK(mesh.mergeVertices(corners[0], corners[1], 0));
    CHECK(mesh.validate());
    CHECK(counts(mesh, 7, 22, 6));
}

TEST_CASE(merge_with_other_diagonal) {
    MeshData mesh = cube();
    auto corners = mesh.getFaceVertices(mesh.getFaceHandles()[0]);
    mesh.connectVertices(corners[1], corners[3]);

    CHECK(mesh.mergeVertices(corners[0], corners[1], 2));
    CHECK(mesh.validate());
}

TEST_CASE(merge_with_triangles_on_both_sides) {
    MeshData mesh = cube();
    auto corners = mesh.getFaceVertices(mesh.getFaceHandles()[0]);
    mesh.connectVertices(corners[0], corners[2]);

    // Triangulate the face on the other side of corners[0] -> corners[1] too.
    FaceHandle other = mesh.getEdge(findEdgeBetween(mesh, corners[1], corners[0]))->face;
    auto otherCorners = mesh.getFaceVertices(other);
    for (size_t i = 0; i < otherCorners.size(); ++i) {
        if (otherCorners[i] == corners[0]) mesh.connectVertices(corners[0], otherCorners[(i + 2) % otherCorners.size()]);
    }

    const size_t facesBefore = mesh.getFaceHandles().size();

    CHECK(mesh.mergeVertices(corners[0], corners[1], 0));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == facesBefore - 2);
}

TEST_CASE(merge_next_to_border_leaves_hole_open) {
    MeshData mesh = cube();
    auto corners = mesh.getFaceVertices(mesh.getFaceHandles()[0]);
    mesh.connectVertices(corners[0], corners[2]);
    mesh.removeFace(mesh.getEdge(findEdgeBetween(mesh, corners[2], corners[1]))->face);

    CHECK(mesh.mergeVertices(corners[0], corners[1], 0));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == 5);
}

TEST_CASE(merge_refuses_pinching_two_borders) {
    MeshData mesh = cube();
    auto faces = mesh.getFaceHandles();
    mesh.removeFace(faces[0]);
    mesh.removeFace(faces[1]);

    EdgeHandle cross = INVALID_EDGE;
    for (EdgeHandle edge : mesh.getEdgeHandles()) {
        if (mesh.isBorder(edge) || mesh.isBorder(mesh.getEdge(edge)->pair)) continue;
        if (mesh.isBorderVertex(mesh.getEdgeOrigin(edge)) && mesh.isBorderVertex(mesh.getEdgeTip(edge))) {
            cross = edge;
            break;
        }
    }

    CHECK(mesh.isValidHandle(cross));
    CHECK(!mesh.mergeVertices(mesh.getEdgeOrigin(cross), mesh.getEdgeTip(cross), 0));
    CHECK(mesh.validate());
}

TEST_CASE(merge_repeatedly_until_refused) {
    MeshData mesh = cube();
    bool merged = true;

    while (merged) {
        merged = false;

        for (EdgeHandle edge : mesh.getEdgeHandles()) {
            if (mesh.mergeVertices(mesh.getEdgeOrigin(edge), mesh.getEdgeTip(edge), 0)) {
                merged = true;
                break;
            }
        }

        if (!mesh.validate()) break;
    }

    CHECK(mesh.validate());
    CHECK(counts(mesh, 3, 6, 2));
}

TEST_CASE(merge_on_extruded_mesh) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    mesh.insertFaceRing(face);

    auto corners = mesh.getFaceVertices(face);
    mesh.connectVertices(corners[0], corners[2]);

    CHECK(mesh.mergeVertices(corners[0], corners[1], 1));
    CHECK(mesh.validate());
}

TEST_CASE(dissolve_edge_to_midpoint) {
    MeshData mesh = cube();
    EdgeHandle edge = mesh.getEdgeHandles()[0];
    const Vec3 middle = (mesh.getVertexPosition(mesh.getEdgeOrigin(edge)) +
                         mesh.getVertexPosition(mesh.getEdgeTip(edge))) / 2.0f;

    VertexHandle survivor = mesh.dissolveEdge(edge);

    CHECK(mesh.isValidHandle(survivor));
    CHECK(mesh.validate());
    CHECK((mesh.getVertexPosition(survivor) - middle).length() < 1e-6f);
}

TEST_CASE(dissolve_cube_face_into_pyramid) {
    MeshData mesh = cube();
    CHECK(mesh.isValidHandle(mesh.dissolveFace(mesh.getFaceHandles()[0])));
    CHECK(mesh.validate());
    CHECK(mesh.getVertexHandles().size() == 5);
    CHECK(mesh.getFaceHandles().size() == 5);
}

TEST_CASE(dissolve_extruded_top) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    mesh.insertFaceRing(face);

    CHECK(mesh.isValidHandle(mesh.dissolveFace(face)));
    CHECK(mesh.validate());
    CHECK(mesh.getFaceHandles().size() == 9);
}

TEST_CASE(dissolve_face_next_to_border) {
    MeshData mesh = cube();
    auto faces = mesh.getFaceHandles();
    mesh.removeFace(faces[1]);

    CHECK(mesh.isValidHandle(mesh.dissolveFace(faces[2])));
    CHECK(mesh.validate());
}

TEST_CASE(dissolve_refused_leaves_mesh_unchanged) {
    MeshData mesh = cube();
    mesh.dissolveFace(mesh.getFaceHandles()[0]);

    FaceHandle base = INVALID_FACE;
    for (FaceHandle face : mesh.getFaceHandles()) {
        if (mesh.getFaceVertices(face).size() == 4) base = face;
    }

    CHECK(!mesh.isValidHandle(mesh.dissolveFace(base)));
    CHECK(mesh.validate());
    CHECK(counts(mesh, 5, 16, 5));
}
