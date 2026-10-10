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

    // The side unrolled across the top half of the texture; the caps as discs below it, top cap left, bottom right
    std::vector<std::vector<u32>> faces;
    std::vector<std::vector<Vec2>> uvs;
    for (u32 i = 0; i < sides; ++i) {
        const u32 next = (i + 1) % sides;
        const f32 u0 = static_cast<f32>(i) / sides;
        const f32 u1 = static_cast<f32>(i + 1) / sides;
        faces.push_back({ i, next, sides + next, sides + i });
        uvs.push_back({ Vec2(u0, 0.5f), Vec2(u1, 0.5f), Vec2(u1, 0.0f), Vec2(u0, 0.0f) });
    }

    const auto disc = [&](f32 centerU, u32 index, f32 flip) {
        const f32 angle = TAU * (index % sides) / sides;
        return Vec2(centerU + 0.25f * std::cos(angle), 0.75f - 0.25f * flip * std::sin(angle));
    };

    std::vector<u32> top;
    std::vector<u32> bottom;
    std::vector<Vec2> topUVs;
    std::vector<Vec2> bottomUVs;
    for (u32 i = 0; i < sides; ++i) {
        top.push_back(sides + i);
        topUVs.push_back(disc(0.25f, i, 1.0f));
        bottom.push_back(sides - 1 - i);
        bottomUVs.push_back(disc(0.75f, sides - 1 - i, -1.0f));
    }

    faces.push_back(top);
    uvs.push_back(topUVs);
    faces.push_back(bottom);
    uvs.push_back(bottomUVs);

    return fromPolygons(positions, faces, uvs);
}
