#include "scene/mesh/mesh_factory.hpp"

#include <cmath>

PackagedMesh MeshFactory::circle(f32 radius, u32 sides) {
    constexpr f32 TAU = 6.28318531f;

    std::vector<Vec3> positions;
    std::vector<u32> face;
    std::vector<Vec2> uvs;

    // Angle runs toward -Z so the face winds counter-clockwise seen from +Y; a disc filling the texture, seen from above
    for (u32 i = 0; i < sides; ++i) {
        const f32 angle = TAU * i / sides;
        positions.push_back(Vec3(radius * std::cos(angle), 0.0f, -radius * std::sin(angle)));
        face.push_back(i);
        uvs.push_back(Vec2(0.5f + 0.5f * std::cos(angle), 0.5f - 0.5f * std::sin(angle)));
    }

    return fromPolygons(positions, { face }, { uvs });
}
