#include "scene/mesh/mesh_factory.hpp"

#include "scene/mesh/mesh_factory.hpp"

PackagedMesh MeshFactory::cube() {
    PackagedMesh mesh;

    mesh.vertices = {
        { Vec3(-0.5f, -0.5f, -0.5f), INVALID_INDEX }, // 0: front left bottom
        { Vec3( 0.5f, -0.5f, -0.5f), INVALID_INDEX }, // 1: front right bottom
        { Vec3( 0.5f,  0.5f, -0.5f), INVALID_INDEX }, // 2: front right top
        { Vec3(-0.5f,  0.5f, -0.5f), INVALID_INDEX }, // 3: front left top

        { Vec3(-0.5f, -0.5f,  0.5f), INVALID_INDEX }, // 4: back left bottom
        { Vec3( 0.5f, -0.5f,  0.5f), INVALID_INDEX }, // 5: back right bottom
        { Vec3( 0.5f,  0.5f,  0.5f), INVALID_INDEX }, // 6: back right top
        { Vec3(-0.5f,  0.5f,  0.5f), INVALID_INDEX }  // 7: back left top
    };

    constexpr u32 FACE_COUNT = 6;
    constexpr u32 EDGES_PER_FACE = 4;
    constexpr u32 EDGE_COUNT = FACE_COUNT * EDGES_PER_FACE;

    mesh.faces.resize(FACE_COUNT);
    mesh.edges.resize(EDGE_COUNT);

    /*
        Each row describes one face in counterclockwise order when viewed
        from outside the cube.

        Since Edge::tip stores the destination vertex, an edge at position i
        travels from faceVertices[i] to faceVertices[i + 1].
    */
    constexpr u32 faceVertices[FACE_COUNT][EDGES_PER_FACE] = {
        { 0, 3, 2, 1 }, // 0: front  (-Z)
        { 4, 5, 6, 7 }, // 1: back   (+Z)
        { 0, 4, 7, 3 }, // 2: left   (-X)
        { 1, 2, 6, 5 }, // 3: right  (+X)
        { 0, 1, 5, 4 }, // 4: bottom (-Y)
        { 3, 7, 6, 2 }  // 5: top    (+Y)
    };

    // Construct each face's circular half-edge loop.
    for (u32 faceIndex = 0; faceIndex < FACE_COUNT; ++faceIndex) {
        const u32 firstEdge = faceIndex * EDGES_PER_FACE;

        mesh.faces[faceIndex].triangulationDirty = true;
        mesh.faces[faceIndex].edge = firstEdge;

        for (u32 localEdge = 0; localEdge < EDGES_PER_FACE; ++localEdge) {
            const u32 edgeIndex = firstEdge + localEdge;

            Edge& edge = mesh.edges[edgeIndex];

            edge.next =
                firstEdge + ((localEdge + 1) % EDGES_PER_FACE);

            edge.prev =
                firstEdge +
                ((localEdge + EDGES_PER_FACE - 1) % EDGES_PER_FACE);

            edge.tip =
                faceVertices[faceIndex]
                            [(localEdge + 1) % EDGES_PER_FACE];

            edge.face = faceIndex;
            edge.pair = INVALID_INDEX;
        }
    }

    /*
        Pair half-edges.

        Because Edge stores only its tip, its origin is the tip of its
        previous edge.
    */
    for (u32 edgeAIndex = 0; edgeAIndex < EDGE_COUNT; ++edgeAIndex) {
        Edge& edgeA = mesh.edges[edgeAIndex];

        const u32 originA = mesh.edges[edgeA.prev].tip;
        const u32 tipA = edgeA.tip;

        for (u32 edgeBIndex = edgeAIndex + 1;
             edgeBIndex < EDGE_COUNT;
             ++edgeBIndex) {

            Edge& edgeB = mesh.edges[edgeBIndex];

            const u32 originB = mesh.edges[edgeB.prev].tip;
            const u32 tipB = edgeB.tip;

            if (originA == tipB && tipA == originB) {
                edgeA.pair = edgeBIndex;
                edgeB.pair = edgeAIndex;
                break;
            }
        }
    }

    // Give each vertex one outgoing half-edge.
    for (u32 edgeIndex = 0; edgeIndex < EDGE_COUNT; ++edgeIndex) {
        const Edge& edge = mesh.edges[edgeIndex];
        const u32 originVertex = mesh.edges[edge.prev].tip;

        if (mesh.vertices[originVertex].edge == INVALID_INDEX) {
            mesh.vertices[originVertex].edge = edgeIndex;
        }
    }

    return mesh;
}