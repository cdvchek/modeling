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

std::vector<u32> MeshData::getEdgeData() const {
    std::vector<u32> edgeData;

    // A closed manifold has two half-edges per rendered edge.
    edgeData.reserve(m_edges.size());

    for (u32 edgeIndex = 0;
         edgeIndex < static_cast<u32>(m_edges.size());
         ++edgeIndex) {

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

        edgeData.push_back(startVertex);
        edgeData.push_back(endVertex);
    }

    return edgeData;
}

std::vector<u32> MeshData::getFaceData() const {
    std::vector<u32> faceData;
    faceData.reserve(m_faces.size() * 6);

    for (u32 i = 0; i < static_cast<u32>(m_faces.size()); ++i) {
        const auto& triangles = getFaceTriangles(i);
        for (const Triangle& triangle : triangles) {
            faceData.push_back(triangle.v0);
            faceData.push_back(triangle.v1);
            faceData.push_back(triangle.v2);
        }
    }

    return faceData;
}