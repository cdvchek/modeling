#include "scene/mesh/mesh_factory.hpp"

#include <cmath>

PackagedMesh MeshFactory::cylinder(f32 radius, f32 height, u32 sides) {
    constexpr f32 TAU = 6.28318531f;
    const f32 halfHeight = height * 0.5f;

    // Bottom ring is 0..sides-1, top ring is sides..2*sides-1
    std::vector<Vec3> positions;
    for (f32 y : { -halfHeight, halfHeight }) {
        for (u32 i = 0; i < sides; ++i) {
            const f32 angle = TAU * i / sides;
            positions.push_back(Vec3(radius * std::cos(angle), y, -radius * std::sin(angle)));
        }
    }

    std::vector<std::vector<u32>> faces;
    for (u32 i = 0; i < sides; ++i) {
        const u32 next = (i + 1) % sides;
        faces.push_back({ i, next, sides + next, sides + i });
    }

    std::vector<u32> top;
    std::vector<u32> bottom;
    for (u32 i = 0; i < sides; ++i) {
        top.push_back(sides + i);
        bottom.push_back(sides - 1 - i);
    }

    faces.push_back(top);
    faces.push_back(bottom);

    return fromPolygons(positions, faces);
}
