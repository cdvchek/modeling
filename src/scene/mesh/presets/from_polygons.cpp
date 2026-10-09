#include "scene/mesh/mesh_factory.hpp"

#include <cassert>
#include <unordered_map>

namespace {
    u64 edgeKey(u32 origin, u32 tip) {
        return (static_cast<u64>(origin) << 32) | tip;
    }
}

PackagedMesh MeshFactory::fromPolygons(const std::vector<Vec3>& positions, const std::vector<std::vector<u32>>& faces,
                                       const std::vector<std::vector<Vec2>>& faceUVs, const std::vector<std::vector<EdgeMark>>& faceMarks) {
    PackagedMesh mesh;

    // 1. Vertices
    std::vector<VertexHandle> vertices;
    vertices.reserve(positions.size());

    for (const Vec3& position : positions) {
        vertices.push_back(mesh.vertices.insert({ position }));
    }

    // 2. Faces and their half-edge loops
    std::unordered_map<u64, EdgeHandle> edgeByEnds;
    std::vector<u32> edgeOrigins;

    for (std::size_t f = 0; f < faces.size(); ++f) {
        const std::vector<u32>& corners = faces[f];
        const u32 sides = static_cast<u32>(corners.size());
        assert(sides >= 3);
        const bool hasUVs = f < faceUVs.size() && faceUVs[f].size() == sides;
        const bool hasMarks = f < faceMarks.size() && faceMarks[f].size() == sides;

        const FaceHandle face = mesh.faces.insert({});
        std::vector<EdgeHandle> loop(sides);

        for (u32 i = 0; i < sides; ++i) {
            const u32 origin = corners[i];
            const u32 tip = corners[(i + 1) % sides];

            Edge edge;
            edge.tip = vertices[tip];
            edge.face = face;
            // A half-edge holds the UV of the corner it points to
            if (hasUVs) edge.uv = faceUVs[f][(i + 1) % sides];
            if (hasMarks) edge.mark = faceMarks[f][(i + 1) % sides];

            loop[i] = mesh.edges.insert(edge);

            [[maybe_unused]] const bool inserted = edgeByEnds.emplace(edgeKey(origin, tip), loop[i]).second;
            assert(inserted && "two faces share a half-edge; check face winding");

            if (edgeOrigins.size() <= loop[i].index) edgeOrigins.resize(loop[i].index + 1);
            edgeOrigins[loop[i].index] = origin;
        }

        for (u32 i = 0; i < sides; ++i) {
            Edge& edge = mesh.edges.get(loop[i]);
            edge.next = loop[(i + 1) % sides];
            edge.prev = loop[(i + sides - 1) % sides];
        }

        mesh.faces.get(face).edge = loop[0];
    }

    // 3. Pair half-edges; unmatched ones get a border half-edge with no face
    std::unordered_map<u32, EdgeHandle> borderByOrigin;
    const std::vector<EdgeHandle> faceEdges = mesh.edges.getActiveHandles();

    for (EdgeHandle handle : faceEdges) {
        if (!mesh.edges.get(handle).pair.isNull()) continue;

        const u32 origin = edgeOrigins[handle.index];
        // Vertices were inserted into a fresh array, so handle index == position index
        const u32 tip = mesh.edges.get(handle).tip.index;

        auto opposite = edgeByEnds.find(edgeKey(tip, origin));
        if (opposite != edgeByEnds.end()) {
            mesh.edges.get(handle).pair = opposite->second;
            mesh.edges.get(opposite->second).pair = handle;
            continue;
        }

        Edge border;
        border.tip = vertices[origin];
        border.face = INVALID_FACE;
        border.pair = handle;
        border.mark = mesh.edges.get(handle).mark;

        const EdgeHandle borderHandle = mesh.edges.insert(border);
        mesh.edges.get(handle).pair = borderHandle;
        borderByOrigin[tip] = borderHandle;
    }

    // 4. Link border half-edges into hole loops
    for (const auto& [origin, handle] : borderByOrigin) {
        const u32 tip = edgeOrigins[mesh.edges.get(handle).pair.index];

        auto next = borderByOrigin.find(tip);
        assert(next != borderByOrigin.end() && "border is not a closed loop");

        mesh.edges.get(handle).next = next->second;
        mesh.edges.get(next->second).prev = handle;
    }

    // 5. Give every vertex one outgoing half-edge, preferring the border one
    for (EdgeHandle handle : mesh.edges.getActiveHandles()) {
        const Edge& edge = mesh.edges.get(handle);
        Vertex& origin = mesh.vertices.get(mesh.edges.get(edge.pair).tip);

        if (origin.edge.isNull() || edge.face.isNull()) origin.edge = handle;
    }

    return mesh;
}
