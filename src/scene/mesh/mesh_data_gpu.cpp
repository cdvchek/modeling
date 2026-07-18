#include "scene/mesh/mesh_data.hpp"

const std::vector<f32> MeshData::getVertexData() const {
    std::vector<f32> vData(m_vertices.size() * 3);
    for (std::size_t i = 0; i < m_vertices.size(); ++i) {
        vData[i * 3 + 0] = m_vertices[i].position.x;
        vData[i * 3 + 1] = m_vertices[i].position.y;
        vData[i * 3 + 2] = m_vertices[i].position.z;
    }
    return vData;
}

const std::vector<u32> MeshData::getEdgeData() const {
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

const std::vector<u32> MeshData::getFaceData() const {
    std::vector<u32> faceData;
    faceData.reserve(m_faces.size() * 6);

    for (const Face& face : m_faces) {
        if (face.edge == INVALID_INDEX ||
            face.edge >= m_edges.size()) {
            continue;
        }

        std::vector<u32> faceVertices;

        u32 edgeIndex = face.edge;

        do {
            const Edge& edge = m_edges[edgeIndex];

            if (edge.tip == INVALID_INDEX ||
                edge.tip >= m_vertices.size()) {
                faceVertices.clear();
                break;
            }

            faceVertices.push_back(edge.tip);

            edgeIndex = edge.next;

            if (edgeIndex == INVALID_INDEX ||
                edgeIndex >= m_edges.size()) {
                faceVertices.clear();
                break;
            }

        } while (edgeIndex != face.edge);

        if (faceVertices.size() < 3) {
            continue;
        }

        for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
            faceData.push_back(faceVertices[0]);
            faceData.push_back(faceVertices[i]);
            faceData.push_back(faceVertices[i + 1]);
        }
    }

    return faceData;
}