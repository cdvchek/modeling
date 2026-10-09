#include "scene/mesh/mesh_data.hpp"

#include <algorithm>
#include "core/math/math_utils.hpp"

VertexData MeshData::getVertexData() const {
    VertexData data;

    data.vertices.reserve(m_vertices.activeSize() * 3);
    data.indexMap.resize(m_vertices.size(), INVALID_INDEX);

    u32 gpuIndex = 0;

    for (u32 i = 0; i < m_vertices.size(); ++i) {
        const VertexHandle handle = m_vertices.getHandle(i);

        if (handle.isNull()) {
            continue;
        }

        const Vertex& vertex = m_vertices.get(handle);

        data.vertices.push_back(vertex.position.x);
        data.vertices.push_back(vertex.position.y);
        data.vertices.push_back(vertex.position.z);

        data.indexMap[i] = gpuIndex;

        ++gpuIndex;
    }

    return data;
}

EdgeData MeshData::getEdgeData(const VertexData& vertexData) const {
    EdgeData data;

    // Two indices per rendered edge.
    data.indices.reserve(m_edges.activeSize());

    u32 renderedIndex = 0;

    for (u32 edgeIndex = 0; edgeIndex < m_edges.size(); ++edgeIndex) {
        const EdgeHandle edgeHandle = m_edges.getHandle(edgeIndex);

        if (edgeHandle.isNull()) {
            continue;
        }

        const Edge& edge = m_edges.get(edgeHandle);

        if (!m_edges.isValid(edge.prev) ||
            !m_vertices.isValid(edge.tip)) {
            continue;
        }

        // Paired half-edges are the same edge; only render one.
        if (!edge.pair.isNull()) {
            if (!m_edges.isValid(edge.pair)) {
                continue;
            }

            if (edgeHandle.index > edge.pair.index) {
                continue;
            }
        }

        const Edge& previousEdge = m_edges.get(edge.prev);

        if (!m_vertices.isValid(previousEdge.tip)) {
            continue;
        }

        const VertexHandle startHandle = previousEdge.tip;
        const VertexHandle endHandle = edge.tip;

        const u32 startVertex = vertexData.indexMap[startHandle.index];
        const u32 endVertex = vertexData.indexMap[endHandle.index];

        if (startVertex == INVALID_INDEX ||
            endVertex == INVALID_INDEX) {
            continue;
        }

        data.indices.push_back(startVertex);
        data.indices.push_back(endVertex);

        data.indexMap.emplace(edgeHandle.index, renderedIndex);
        if (!edge.pair.isNull()) data.indexMap.emplace(edge.pair.index, renderedIndex);

        renderedIndex += 2;
    }

    return data;
}

FaceData MeshData::getFaceData(const FaceGroupOf& groupOf) const {
    FaceData data;

    data.indexMap.resize(m_faces.size() * 2, INVALID_INDEX);
    data.indices.reserve(m_faces.activeSize() * 6);
    data.vertices.reserve(m_faces.activeSize() * 6 * FaceData::FLOATS_PER_VERTEX);

    // Faces in group order (the object's material first, then by handle), slot order within a group
    struct Slot {
        MaterialHandle group;
        u32 index;
    };
    std::vector<Slot> slots;
    slots.reserve(m_faces.activeSize());
    for (u32 i = 0; i < m_faces.size(); ++i) {
        const FaceHandle faceHandle = m_faces.getHandle(i);
        if (faceHandle.isNull()) continue;
        const MaterialHandle own = m_faces.get(faceHandle).material;
        slots.push_back({ groupOf ? groupOf(own) : own, i });
    }
    const auto key = [](MaterialHandle handle) {
        return handle.isNull() ? 0ull : (static_cast<u64>(handle.index) << 32 | handle.generation) + 1;
    };
    std::stable_sort(slots.begin(), slots.end(), [&](const Slot& a, const Slot& b) { return key(a.group) < key(b.group); });

    u32 indexCount = 0;

    for (const Slot& slot : slots) {
        const u32 i = slot.index;
        const FaceHandle faceHandle = m_faces.getHandle(i);

        if (data.groups.empty() || !(data.groups.back().material == slot.group)) {
            data.groups.push_back({ slot.group, indexCount, 0 });
        }

        // Each corner is its own vertex, so the indices just count up
        const std::size_t before = data.vertices.size();
        appendFaceCorners(faceHandle, data.vertices);
        const u32 faceIndexCount = static_cast<u32>((data.vertices.size() - before) / FaceData::FLOATS_PER_VERTEX);
        for (u32 k = 0; k < faceIndexCount; ++k) data.indices.push_back(indexCount + k);

        data.indexMap[i * 2]     = indexCount;
        data.indexMap[i * 2 + 1] = faceIndexCount;

        indexCount += faceIndexCount;
        data.groups.back().indexCount += faceIndexCount;
    }

    return data;
}

void MeshData::appendFaceCorners(FaceHandle handle, std::vector<f32>& out) const {
    if (!m_faces.isValid(handle)) return;
    const Vec3 faceNormal = getFaceNormal(handle);
    const bool flat = m_shading == ShadingMode::Flat;
    // A flat face uses its own normal on every triangle, so its corners match exactly
    const bool planar = flat && isFacePlanar(handle);

    // Triangles name vertices; this face's UV (and smooth normal) at each comes from the half-edge ending there
    struct Corner { u32 vertex; Vec2 uv; Vec3 normal; };
    std::vector<Corner> corners;
    for (EdgeHandle edge : getFaceEdges(handle)) {
        const Edge& half = m_edges.get(edge);
        corners.push_back({ half.tip.index, half.uv, flat ? faceNormal : getCornerNormal(edge) });
    }
    const auto cornerOf = [&](VertexHandle vertex) -> const Corner* {
        for (const Corner& corner : corners) if (corner.vertex == vertex.index) return &corner;
        return nullptr;
    };

    // Every triangle gets its own corners so a non-planar flat face shows its fold
    for (const Triangle& triangle : getFaceTriangles(handle)) {
        if (!m_vertices.isValid(triangle.v0) || !m_vertices.isValid(triangle.v1) || !m_vertices.isValid(triangle.v2)) continue;

        const VertexHandle vertices[3] = { triangle.v0, triangle.v1, triangle.v2 };
        const Vec3 positions[3] = {
            m_vertices.get(triangle.v0).position,
            m_vertices.get(triangle.v1).position,
            m_vertices.get(triangle.v2).position
        };

        Vec3 triangleNormal = faceNormal;
        if (flat && !planar) {
            const Vec3 cross = Vec3::cross(positions[1] - positions[0], positions[2] - positions[0]);
            const f32 length = cross.length();
            if (length > Math::EPSILON) triangleNormal = cross / length;
        }

        for (u32 k = 0; k < 3; ++k) {
            const Corner* corner = cornerOf(vertices[k]);
            const Vec2 uv = corner ? corner->uv : Vec2();
            const Vec3 normal = flat || !corner ? triangleNormal : corner->normal;
            out.insert(out.end(), { positions[k].x, positions[k].y, positions[k].z, normal.x, normal.y, normal.z, uv.x, uv.y });
        }
    }
}
