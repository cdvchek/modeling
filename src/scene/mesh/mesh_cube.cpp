#include "scene/mesh/mesh_factory.hpp"

MeshData MeshFactory::createCube() {
    MeshData mesh;

    mesh.vertices = {
        { Vec3(-0.5f, -0.5f, -0.5f) },
        { Vec3( 0.5f, -0.5f, -0.5f) },
        { Vec3( 0.5f,  0.5f, -0.5f) },
        { Vec3(-0.5f,  0.5f, -0.5f) },

        { Vec3(-0.5f, -0.5f,  0.5f) },
        { Vec3( 0.5f, -0.5f,  0.5f) },
        { Vec3( 0.5f,  0.5f,  0.5f) },
        { Vec3(-0.5f,  0.5f,  0.5f) }
    };

    mesh.indices = {
        4, 5, 6, 6, 7, 4,
        1, 0, 3, 3, 2, 1,
        0, 4, 7, 7, 3, 0,
        5, 1, 2, 2, 6, 5,
        3, 7, 6, 6, 2, 3,
        0, 1, 5, 5, 4, 0
    };

    return mesh;
}