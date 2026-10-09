#include "scene/mesh/mesh_factory.hpp"

#include <cmath>

PackagedMesh MeshFactory::uvSphere(f32 radius, u32 segments, u32 rings) {
    constexpr f32 PI = 3.14159265f;
    constexpr f32 TAU = 6.28318531f;

    // Top pole, then rings 1..rings-1 from top to bottom, then bottom pole
    std::vector<Vec3> positions;
    positions.push_back(Vec3(0.0f, radius, 0.0f));

    for (u32 ring = 1; ring < rings; ++ring) {
        const f32 polar = PI * ring / rings;
        const f32 y = radius * std::cos(polar);
        const f32 ringRadius = radius * std::sin(polar);

        for (u32 i = 0; i < segments; ++i) {
            const f32 angle = TAU * i / segments;
            positions.push_back(Vec3(ringRadius * std::cos(angle), y, -ringRadius * std::sin(angle)));
        }
    }

    positions.push_back(Vec3(0.0f, -radius, 0.0f));

    const u32 topPole = 0;
    const u32 bottomPole = static_cast<u32>(positions.size()) - 1;
    const auto ringVertex = [segments](u32 ring, u32 i) { return 1 + (ring - 1) * segments + i % segments; };

    // Longitude across, latitude down; each pole corner sits above the middle of its triangle
    const auto uv = [&](u32 ring, f32 i) { return Vec2(i / segments, static_cast<f32>(ring) / rings); };

    std::vector<std::vector<u32>> faces;
    std::vector<std::vector<Vec2>> uvs;

    for (u32 i = 0; i < segments; ++i) {
        faces.push_back({ topPole, ringVertex(1, i), ringVertex(1, i + 1) });
        uvs.push_back({ uv(0, i + 0.5f), uv(1, i), uv(1, i + 1.0f) });
    }

    for (u32 ring = 1; ring + 1 < rings; ++ring) {
        for (u32 i = 0; i < segments; ++i) {
            faces.push_back({ ringVertex(ring, i), ringVertex(ring + 1, i), ringVertex(ring + 1, i + 1), ringVertex(ring, i + 1) });
            uvs.push_back({ uv(ring, i), uv(ring + 1, i), uv(ring + 1, i + 1.0f), uv(ring, i + 1.0f) });
        }
    }

    for (u32 i = 0; i < segments; ++i) {
        faces.push_back({ ringVertex(rings - 1, i), bottomPole, ringVertex(rings - 1, i + 1) });
        uvs.push_back({ uv(rings - 1, i), uv(rings, i + 0.5f), uv(rings - 1, i + 1.0f) });
    }

    return fromPolygons(positions, faces, uvs);
}
