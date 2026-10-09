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

    // A cross on a 4 x 4 grid of cells, each face upright as seen from outside: the sides in a row (+X, -Z, -X, +Z),
    // the top above -Z and the bottom below it, sharing the edges they share on the cube
    struct Unfold {
        u32 column, row;
        Vec3 right, up;
    };
    const Unfold unfolds[6] = {
        { 1, 1, Vec3(-1, 0, 0), Vec3(0, 1, 0) },     // front (-Z)
        { 3, 1, Vec3(1, 0, 0), Vec3(0, 1, 0) },      // back (+Z)
        { 2, 1, Vec3(0, 0, 1), Vec3(0, 1, 0) },      // left (-X)
        { 0, 1, Vec3(0, 0, -1), Vec3(0, 1, 0) },     // right (+X)
        { 1, 2, Vec3(-1, 0, 0), Vec3(0, 0, -1) },    // bottom (-Y)
        { 1, 0, Vec3(-1, 0, 0), Vec3(0, 0, 1) },     // top (+Y)
    };
    constexpr f32 CELL = 0.25f;

    std::vector<std::vector<Vec2>> uvs;
    for (u32 f = 0; f < faces.size(); ++f) {
        const Unfold& unfold = unfolds[f];
        std::vector<Vec2> corners;
        for (u32 corner : faces[f]) {
            const Vec3& p = positions[corner];
            corners.push_back(Vec2((unfold.column + Vec3::dot(p, unfold.right) + 0.5f) * CELL,
                                   (unfold.row + 0.5f - Vec3::dot(p, unfold.up)) * CELL));
        }
        uvs.push_back(corners);
    }

    return fromPolygons(positions, faces, uvs);
}
