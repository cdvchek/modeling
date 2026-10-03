#include "test.hpp"
#include "mesh_helpers.hpp"

TEST_CASE(cube_edge_loop_stops_at_three_edge_corners) {
    MeshData mesh = cube();
    CHECK(mesh.getEdgeLoop(mesh.getEdgeHandles()[0]).size() == 1);
}

TEST_CASE(cube_edge_ring_and_face_loop_go_around) {
    MeshData mesh = cube();
    EdgeHandle edge = mesh.getEdgeHandles()[0];

    auto ring = mesh.getEdgeRing(edge);
    CHECK(ring.size() == 4);
    CHECK(distinctEdges(mesh, ring));
    CHECK(mesh.getFaceLoop(edge).size() == 4);
}

TEST_CASE(extrusion_loops_and_rings) {
    MeshData mesh = cube();
    FaceHandle top = mesh.getFaceHandles()[0];
    extrude(mesh, top);

    // An edge between two 4-edge corners runs along the old outline; one with a single 4-edge end runs up the side.
    EdgeHandle outline = INVALID_EDGE;
    EdgeHandle vertical = INVALID_EDGE;

    for (EdgeHandle edge : mesh.getEdgeHandles()) {
        const bool originFour = mesh.getOutgoingEdges(mesh.getEdgeOrigin(edge)).size() == 4;
        const bool tipFour = mesh.getOutgoingEdges(mesh.getEdgeTip(edge)).size() == 4;

        if (originFour && tipFour && outline.isNull()) outline = edge;
        if (originFour != tipFour && vertical.isNull()) vertical = edge;
    }

    auto loop = mesh.getEdgeLoop(outline);
    CHECK(loop.size() == 4);
    CHECK(distinctEdges(mesh, loop));

    auto ring = mesh.getEdgeRing(vertical);
    CHECK(ring.size() == 4);
    CHECK(distinctEdges(mesh, ring));

    auto sides = mesh.getFaceLoop(vertical);
    CHECK(sides.size() == 4);
    CHECK(std::find(sides.begin(), sides.end(), top) == sides.end());

    auto across = mesh.getFaceLoop(mesh.getFace(top)->edge);
    CHECK(across.size() == 6);
    CHECK(std::find(across.begin(), across.end(), top) != across.end());
}

TEST_CASE(border_edge_loop_is_the_hole) {
    MeshData mesh = cube();
    FaceHandle face = mesh.getFaceHandles()[0];
    EdgeHandle edge = mesh.getFace(face)->edge;
    mesh.removeFace(face);

    CHECK(mesh.getEdgeLoop(edge).size() == 4);
    CHECK(mesh.getEdgeRing(edge).size() == 4);
}

TEST_CASE(edge_highlight_map_covers_both_halves) {
    MeshData mesh = cube();
    EdgeData data = mesh.getEdgeData(mesh.getVertexData());

    for (EdgeHandle edge : mesh.getEdgeHandles()) {
        CHECK(data.indexMap.count(edge.index) == 1);
    }
}
