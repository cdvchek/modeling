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

    // The side unrolled across the top half (the apex along the top edge), the base as a disc below
    std::vector<std::vector<u32>> faces;
    std::vector<std::vector<Vec2>> uvs;
    for (u32 i = 0; i < sides; ++i) {
        const f32 u0 = static_cast<f32>(i) / sides;
        const f32 u1 = static_cast<f32>(i + 1) / sides;
        faces.push_back({ i, (i + 1) % sides, apex });
        uvs.push_back({ Vec2(u0, 0.5f), Vec2(u1, 0.5f), Vec2((u0 + u1) * 0.5f, 0.0f) });
    }

    std::vector<u32> base;
    std::vector<Vec2> baseUVs;
    for (u32 i = 0; i < sides; ++i) {
        const u32 index = sides - 1 - i;
        const f32 angle = TAU * index / sides;
        base.push_back(index);
        baseUVs.push_back(Vec2(0.25f + 0.25f * std::cos(angle), 0.75f + 0.25f * std::sin(angle)));
    }
    faces.push_back(base);
    uvs.push_back(baseUVs);

    return fromPolygons(positions, faces, uvs);
}
