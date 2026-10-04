#include "scene/mesh/mesh_factory.hpp"

#include <cmath>

PackagedMesh MeshFactory::cone(f32 radius, f32 height, u32 sides) {
    constexpr f32 TAU = 6.28318531f;
    const f32 halfHeight = height * 0.5f;

    // Base ring is 0..sides-1, apex is sides
    std::vector<Vec3> positions;
    for (u32 i = 0; i < sides; ++i) {
        const f32 angle = TAU * i / sides;
        positions.push_back(Vec3(radius * std::cos(angle), -halfHeight, -radius * std::sin(angle)));
    }
    positions.push_back(Vec3(0.0f, halfHeight, 0.0f));

    const u32 apex = sides;

    std::vector<std::vector<u32>> faces;
    for (u32 i = 0; i < sides; ++i) {
        faces.push_back({ i, (i + 1) % sides, apex });
    }

    std::vector<u32> base;
    for (u32 i = 0; i < sides; ++i) {
        base.push_back(sides - 1 - i);
    }
    faces.push_back(base);

    return fromPolygons(positions, faces);
}
