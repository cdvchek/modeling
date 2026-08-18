#include "scene/mesh/mesh_data.hpp"

std::vector<f32> MeshData::getVertexData() const {
    std::vector<f32> vData(m_vertices.size() * 3);
    for (std::size_t i = 0; i < m_vertices.size(); ++i) {
        vData[i * 3 + 0] = m_vertices[i].position.x;
        vData[i * 3 + 1] = m_vertices[i].position.y;
        vData[i * 3 + 2] = m_vertices[i].position.z;
    }
    return vData;
}

EdgeData MeshData::getEdgeData() const {
    EdgeData data;
    
    // A closed manifold has two half-edges per rendered edge.
    data.indices.reserve(m_edges.size());
    data.indexMap.reserve(m_edges.size() / 2);

    u32 renderedIndex = 0;
    for (u32 edgeIndex = 0; edgeIndex < static_cast<u32>(m_edges.size()); ++edgeIndex) {
        const Edge& edge = m_edges[edgeIndex];

        if (edge.prev == INVALID_INDEX ||
            edge.tip == INVALID_INDEX ||
            edge.prev >= m_edges.size() ||
            edge.tip >= m_vertices.size()) {
            continue;
        }

        /*
         * Paired half-edges describe the same physical edge.
         * Only output the half-edge with the lower index.
         *
         * An unpaired half-edge is a boundary edge and should still
         * be included.
         */
        if (edge.pair != INVALID_INDEX) {
            if (edge.pair >= m_edges.size()) {
                continue;
            }

            if (edgeIndex > edge.pair) {
                continue;
            }
        }

        const Edge& previousEdge = m_edges[edge.prev];

        if (previousEdge.tip == INVALID_INDEX ||
            previousEdge.tip >= m_vertices.size()) {
            continue;
        }

        const u32 startVertex = previousEdge.tip;
        const u32 endVertex = edge.tip;

        data.indices.push_back(startVertex);
        data.indices.push_back(endVertex);
        data.indexMap.emplace(edgeIndex, renderedIndex);
        renderedIndex++;
    }

    return data;
}

FaceData MeshData::getFaceData() const {
    FaceData data;

    data.indexMap.reserve(m_faces.size() * 2);
    data.indices.reserve(m_faces.size() * 6);

    u32 indexCount = 0;

    for (u32 i = 0; i < static_cast<u32>(m_faces.size()); ++i) {
        const auto& triangles = getFaceTriangles(i);

        u32 faceIndexCount =
            static_cast<u32>(triangles.size()) * 3;

        data.indexMap.push_back(indexCount);
        data.indexMap.push_back(faceIndexCount);

        for (const Triangle& triangle : triangles) {
            data.indices.push_back(triangle.v0);
            data.indices.push_back(triangle.v1);
            data.indices.push_back(triangle.v2);
        }

        indexCount += faceIndexCount;
    }

    return data;
}