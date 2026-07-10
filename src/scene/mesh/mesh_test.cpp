#include "scene/mesh/mesh_factory.hpp"

MeshData MeshFactory::createTest() {
    MeshData mesh;

    mesh.setVertices({
        { Vec3(0.0f, 0.0f, 0.0f) },
        { Vec3(0.0f, 0.3f, 0.0f) },
        { Vec3(0.0f, 0.3f, 0.1f) }
    });

    mesh.setEdges({
        { 0, 1 },
        { 1, 2 }
    });

    return mesh;
}