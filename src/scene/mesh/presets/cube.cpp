#include "scene/mesh/mesh_factory.hpp"
#include <types>
#include <array>

PackagedMesh MeshFactory::cube() {
    PackagedMesh mesh;

    constexpr u32 VERTEX_COUNT = 8;
    constexpr u32 FACE_COUNT = 6;
    constexpr u32 EDGES_PER_FACE = 4;
    constexpr u32 EDGE_COUNT = FACE_COUNT * EDGES_PER_FACE;

    std::array<VertexHandle, VERTEX_COUNT> vertices;

    vertices[0] = mesh.vertices.insert({ Vec3(-0.5f, -0.5f, -0.5f) });
    vertices[1] = mesh.vertices.insert({ Vec3(0.5f, -0.5f, -0.5f) });
    vertices[2] = mesh.vertices.insert({ Vec3(0.5f, 0.5f, -0.5f) });
    vertices[3] = mesh.vertices.insert({ Vec3(-0.5f, 0.5f, -0.5f) });
    vertices[4] = mesh.vertices.insert({ Vec3(-0.5f, -0.5f, 0.5f) });
    vertices[5] = mesh.vertices.insert({ Vec3(0.5f, -0.5f, 0.5f) });
    vertices[6] = mesh.vertices.insert({ Vec3(0.5f, 0.5f, 0.5f) });
    vertices[7] = mesh.vertices.insert({ Vec3(-0.5f, 0.5f, 0.5f) });

    constexpr u32 faceVertices[FACE_COUNT][EDGES_PER_FACE] = {
        { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 4, 7, 3 }, // front (-Z) // back   (+Z) // left (-X)
        { 1, 2, 6, 5 }, { 0, 1, 5, 4 }, { 3, 7, 6, 2 }  // right (+X) // bottom (-Y) // top  (+Y)
    };

    std::array<FaceHandle, FACE_COUNT> faces;
    std::array<std::array<EdgeHandle, EDGES_PER_FACE>, FACE_COUNT> edges;

    // Create faces and their half-edges.
    for (u32 faceIndex = 0; faceIndex < FACE_COUNT; ++faceIndex) {
        faces[faceIndex] = mesh.faces.insert({});

        for (u32 localEdge = 0; localEdge < EDGES_PER_FACE; ++localEdge) {
            const u32 tipIndex = faceVertices[faceIndex][(localEdge + 1) % EDGES_PER_FACE];

            Edge edge;

            edge.tip = vertices[tipIndex];
            edge.face = faces[faceIndex];

            edges[faceIndex][localEdge] = mesh.edges.insert(edge);
        }

        // Wire the circular face loop.
        for (u32 localEdge = 0; localEdge < EDGES_PER_FACE; ++localEdge) {
            Edge& edge = mesh.edges.get(edges[faceIndex][localEdge]);

            edge.next = edges[faceIndex][(localEdge + 1) % EDGES_PER_FACE];

            edge.prev = edges[faceIndex][(localEdge + EDGES_PER_FACE - 1) % EDGES_PER_FACE];
        }

        mesh.faces.get(faces[faceIndex]).edge = edges[faceIndex][0];
    }

    // Pair opposite half-edges.
    for (u32 faceA = 0; faceA < FACE_COUNT; ++faceA) {
        for (u32 localA = 0; localA < EDGES_PER_FACE; ++localA) {
            const EdgeHandle handleA = edges[faceA][localA];
            Edge& edgeA = mesh.edges.get(handleA);

            if (!edgeA.pair.isNull()) continue;

            const VertexHandle originA = mesh.edges.get(edgeA.prev).tip;
            const VertexHandle tipA = edgeA.tip;

            for (u32 faceB = 0; faceB < FACE_COUNT; ++faceB) {
                for (u32 localB = 0; localB < EDGES_PER_FACE; ++localB) {

                    const EdgeHandle handleB = edges[faceB][localB];

                    if (handleA == handleB) continue;

                    Edge& edgeB = mesh.edges.get(handleB);

                    if (!edgeB.pair.isNull()) continue;

                    const VertexHandle originB = mesh.edges.get(edgeB.prev).tip;
                    const VertexHandle tipB = edgeB.tip;

                    if (originA == tipB && tipA == originB) {
                        edgeA.pair = handleB;
                        edgeB.pair = handleA;
                        break;
                    }
                }

                if (!edgeA.pair.isNull()) break;
            }
        }
    }

    // Give every vertex one outgoing half-edge.
    for (u32 faceIndex = 0; faceIndex < FACE_COUNT; ++faceIndex) {
        for (u32 localEdge = 0; localEdge < EDGES_PER_FACE; ++localEdge) {
            const EdgeHandle handle = edges[faceIndex][localEdge];
            const Edge& edge = mesh.edges.get(handle);

            const VertexHandle origin = mesh.edges.get(edge.prev).tip;

            Vertex& vertex = mesh.vertices.get(origin);

            if (vertex.edge.isNull()) vertex.edge = handle;
        }
    }

    return mesh;
}