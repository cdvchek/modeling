#include "scene/mesh/mesh_data.hpp"
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

FaceData MeshData::getFaceData() const {
    FaceData data;

    data.indexMap.resize(m_faces.size() * 2, INVALID_INDEX);
    data.indices.reserve(m_faces.activeSize() * 6);
    data.vertices.reserve(m_faces.activeSize() * 6 * FaceData::FLOATS_PER_VERTEX);

    u32 indexCount = 0;

    for (u32 i = 0; i < m_faces.size(); ++i) {
        const FaceHandle faceHandle = m_faces.getHandle(i);

        if (faceHandle.isNull()) {
            continue;
        }

        const Vec3 faceNormal = getFaceNormal(faceHandle);
        const auto& triangles = getFaceTriangles(faceHandle);
        const u32 faceStart = static_cast<u32>(data.indices.size());

        // Every triangle gets its own corners so a non-planar face shows its fold
        for (const Triangle& triangle : triangles) {
            if (!m_vertices.isValid(triangle.v0) || !m_vertices.isValid(triangle.v1) || !m_vertices.isValid(triangle.v2)) continue;

            const Vec3 positions[3] = {
                m_vertices.get(triangle.v0).position,
                m_vertices.get(triangle.v1).position,
                m_vertices.get(triangle.v2).position
            };

            const Vec3 cross = Vec3::cross(positions[1] - positions[0], positions[2] - positions[0]);
            const f32 length = cross.length();
            const Vec3 normal = length > Math::EPSILON ? cross / length : faceNormal;

            for (const Vec3& position : positions) {
                data.indices.push_back(static_cast<u32>(data.vertices.size() / FaceData::FLOATS_PER_VERTEX));
                data.vertices.insert(data.vertices.end(), {
                    position.x, position.y, position.z,
                    normal.x, normal.y, normal.z
                });
            }
        }

        const u32 faceIndexCount = static_cast<u32>(data.indices.size()) - faceStart;

        data.indexMap[i * 2]     = indexCount;
        data.indexMap[i * 2 + 1] = faceIndexCount;

        indexCount += faceIndexCount;
    }

    return data;
}
