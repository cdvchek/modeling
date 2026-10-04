#include "scene/mesh/mesh_factory.hpp"

PackagedMesh MeshFactory::cube() {
    const std::vector<Vec3> positions = {
        Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, -0.5f), Vec3(-0.5f, 0.5f, -0.5f),
        Vec3(-0.5f, -0.5f,  0.5f), Vec3(0.5f, -0.5f,  0.5f), Vec3(0.5f, 0.5f,  0.5f), Vec3(-0.5f, 0.5f,  0.5f)
    };

    const std::vector<std::vector<u32>> faces = {
        { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 4, 7, 3 }, // front (-Z) // back   (+Z) // left (-X)
        { 1, 2, 6, 5 }, { 0, 1, 5, 4 }, { 3, 7, 6, 2 }  // right (+X) // bottom (-Y) // top  (+Y)
    };

    return fromPolygons(positions, faces);
}
