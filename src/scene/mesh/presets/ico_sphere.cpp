#include "scene/mesh/mesh_factory.hpp"

#include <cmath>
#include <unordered_map>

namespace {
    u32 midpoint(std::vector<Vec3>& positions, std::unordered_map<u64, u32>& cache, u32 a, u32 b) {
        const u64 key = a < b ? (static_cast<u64>(a) << 32) | b : (static_cast<u64>(b) << 32) | a;

        auto it = cache.find(key);
        if (it != cache.end()) return it->second;

        positions.push_back(((positions[a] + positions[b]) * 0.5f).normalized());
        const u32 index = static_cast<u32>(positions.size()) - 1;
        cache.emplace(key, index);
        return index;
    }
}

PackagedMesh MeshFactory::icoSphere(f32 radius, u32 subdivisions) {
    const f32 t = (1.0f + std::sqrt(5.0f)) * 0.5f;

    std::vector<Vec3> positions = {
        Vec3(-1,  t,  0), Vec3( 1,  t,  0), Vec3(-1, -t,  0), Vec3( 1, -t,  0),
        Vec3( 0, -1,  t), Vec3( 0,  1,  t), Vec3( 0, -1, -t), Vec3( 0,  1, -t),
        Vec3( t,  0, -1), Vec3( t,  0,  1), Vec3(-t,  0, -1), Vec3(-t,  0,  1)
    };
    for (Vec3& position : positions) position = position.normalized();

    std::vector<std::vector<u32>> faces = {
        { 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
        { 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
        { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
        { 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 }
    };

    // Split every triangle into four, pushing new points onto the unit sphere
    for (u32 level = 0; level < subdivisions; ++level) {
        std::unordered_map<u64, u32> cache;
        std::vector<std::vector<u32>> split;

        for (const std::vector<u32>& face : faces) {
            const u32 ab = midpoint(positions, cache, face[0], face[1]);
            const u32 bc = midpoint(positions, cache, face[1], face[2]);
            const u32 ca = midpoint(positions, cache, face[2], face[0]);

            split.push_back({ face[0], ab, ca });
            split.push_back({ face[1], bc, ab });
            split.push_back({ face[2], ca, bc });
            split.push_back({ ab, bc, ca });
        }

        faces = std::move(split);
    }

    for (Vec3& position : positions) position = position * radius;

    return fromPolygons(positions, faces);
}
