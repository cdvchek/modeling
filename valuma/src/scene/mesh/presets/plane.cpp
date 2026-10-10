#include "scene/mesh/mesh_factory.hpp"

PackagedMesh MeshFactory::plane(f32 size, u32 divisions) {
    const u32 rowLength = divisions + 1;
    const f32 step = size / static_cast<f32>(divisions);
    const f32 start = -size * 0.5f;

    std::vector<Vec3> positions;
    for (u32 z = 0; z <= divisions; ++z) {
        for (u32 x = 0; x <= divisions; ++x) {
            positions.push_back(Vec3(start + step * x, 0.0f, start + step * z));
        }
    }

    // Seen from above with -Z up: the whole texture over the plane
    const auto uvOf = [&](u32 index) {
        const Vec3& p = positions[index];
        return Vec2((p.x - start) / size, (p.z - start) / size);
    };

    // Wound to face +Y
    std::vector<std::vector<u32>> faces;
    std::vector<std::vector<Vec2>> uvs;
    for (u32 z = 0; z < divisions; ++z) {
        for (u32 x = 0; x < divisions; ++x) {
            const u32 corner = z * rowLength + x;
            faces.push_back({ corner, corner + rowLength, corner + rowLength + 1, corner + 1 });
            uvs.push_back({ uvOf(corner), uvOf(corner + rowLength), uvOf(corner + rowLength + 1), uvOf(corner + 1) });
        }
    }

    return fromPolygons(positions, faces, uvs);
}
