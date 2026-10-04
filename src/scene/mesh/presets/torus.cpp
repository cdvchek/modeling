#include "scene/mesh/mesh_factory.hpp"

#include <cmath>

PackagedMesh MeshFactory::torus(f32 majorRadius, f32 minorRadius, u32 segments, u32 sides) {
    constexpr f32 TAU = 6.28318531f;

    // segments go around the Y axis, sides go around the tube
    std::vector<Vec3> positions;
    for (u32 i = 0; i < segments; ++i) {
        const f32 around = TAU * i / segments;

        for (u32 j = 0; j < sides; ++j) {
            const f32 tube = TAU * j / sides;
            const f32 distance = majorRadius + minorRadius * std::cos(tube);

            positions.push_back(Vec3(distance * std::cos(around), minorRadius * std::sin(tube), -distance * std::sin(around)));
        }
    }

    const auto vertex = [segments, sides](u32 i, u32 j) { return (i % segments) * sides + j % sides; };

    std::vector<std::vector<u32>> faces;
    for (u32 i = 0; i < segments; ++i) {
        for (u32 j = 0; j < sides; ++j) {
            faces.push_back({ vertex(i, j), vertex(i + 1, j), vertex(i + 1, j + 1), vertex(i, j + 1) });
        }
    }

    return fromPolygons(positions, faces);
}
