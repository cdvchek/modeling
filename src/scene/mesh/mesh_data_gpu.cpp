#include "scene/mesh/mesh_data.hpp"

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
    data.vertices.reserve(m_faces.activeSize() * 4 * FaceData::FLOATS_PER_VERTEX);

    u32 indexCount = 0;

    for (u32 i = 0; i < m_faces.size(); ++i) {
        const FaceHandle faceHandle = m_faces.getHandle(i);

        if (faceHandle.isNull()) {
            continue;
        }

        // Each face gets its own corners so every corner carries the face normal
        const std::vector<VertexHandle> corners = getFaceVertices(faceHandle);
        const Vec3 normal = getFaceNormal(faceHandle);
        const u32 firstCorner = static_cast<u32>(data.vertices.size() / FaceData::FLOATS_PER_VERTEX);

        for (VertexHandle corner : corners) {
            const Vec3 position = getVertexPosition(corner);

            data.vertices.insert(data.vertices.end(), {
                position.x, position.y, position.z,
                normal.x, normal.y, normal.z
            });
        }

        const auto cornerIndex = [&](VertexHandle vertex) {
            for (u32 c = 0; c < corners.size(); ++c) {
                if (corners[c] == vertex) return firstCorner + c;
            }
            return INVALID_INDEX;
        };

        const auto& triangles = getFaceTriangles(faceHandle);
        const u32 faceStart = static_cast<u32>(data.indices.size());

        for (const Triangle& triangle : triangles) {
            const u32 v0 = cornerIndex(triangle.v0);
            const u32 v1 = cornerIndex(triangle.v1);
            const u32 v2 = cornerIndex(triangle.v2);

            if (v0 == INVALID_INDEX || v1 == INVALID_INDEX || v2 == INVALID_INDEX) continue;

            data.indices.push_back(v0);
            data.indices.push_back(v1);
            data.indices.push_back(v2);
        }

        const u32 faceIndexCount = static_cast<u32>(data.indices.size()) - faceStart;

        data.indexMap[i * 2]     = indexCount;
        data.indexMap[i * 2 + 1] = faceIndexCount;

        indexCount += faceIndexCount;
    }

    return data;
}
