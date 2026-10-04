#include "scene/mesh/mesh_factory.hpp"

#include <cmath>

PackagedMesh MeshFactory::circle(f32 radius, u32 sides) {
    constexpr f32 TAU = 6.28318531f;

    std::vector<Vec3> positions;
    std::vector<u32> face;

    // Angle runs toward -Z so the face winds counter-clockwise seen from +Y
    for (u32 i = 0; i < sides; ++i) {
        const f32 angle = TAU * i / sides;
        positions.push_back(Vec3(radius * std::cos(angle), 0.0f, -radius * std::sin(angle)));
        face.push_back(i);
    }

    return fromPolygons(positions, { face });
}
