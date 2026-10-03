#include "scene/mesh/mesh_data.hpp"

#include <algorithm>

std::vector<VertexHandle> MeshData::getFaceVertices(FaceHandle handle) const {
    const Face* face = m_faces.tryGet(handle);
    if (!face) return {};

    const EdgeHandle firstEdgeHandle = face->edge;
    if (!m_edges.isValid(firstEdgeHandle)) return {};

    std::vector<VertexHandle> vertices;
    EdgeHandle edgeHandle = firstEdgeHandle;

    do {
        if (!m_edges.isValid(edgeHandle)) return {};

        const Edge& edge = m_edges.get(edgeHandle);

        if (!m_vertices.isValid(edge.tip)) return {};

        vertices.push_back(edge.tip);
        edgeHandle = edge.next;

        // A valid face cannot contain more half-edges than exist globally.
        if (static_cast<u32>(vertices.size()) > m_edges.size()) return {};
    } while (!(edgeHandle == firstEdgeHandle));

    return vertices;
}

std::vector<EdgeHandle> MeshData::getFaceEdges(FaceHandle handle) const {
    std::vector<EdgeHandle> edges;

    const Face* face = m_faces.tryGet(handle);
    if (!face || !m_edges.isValid(face->edge)) return edges;

    EdgeHandle first = face->edge;
    EdgeHandle current = first;

    do {
        const Edge* edge = m_edges.tryGet(current);
        if (!edge || !(edge->face == handle)) return {};

        edges.push_back(current);
        
        current = edge->next;
        if (!m_edges.isValid(current)) return {};

        if (edges.size() > m_edges.activeSize()) return {};
    } while (!(current == first));

    return edges;
}

EdgeHandle MeshData::findOutgoingEdge(VertexHandle handle, const std::vector<EdgeHandle>& excluded) const {
    const Vertex* vertex = m_vertices.tryGet(handle);
    if (!vertex) return INVALID_EDGE;

    // First try local half-edge traversal.
    if (m_edges.isValid(vertex->edge)) {
        EdgeHandle first = vertex->edge;
        EdgeHandle current = first;

        do {
            if (std::find(excluded.begin(), excluded.end(), current) == excluded.end()) return current;

            const Edge* edge = m_edges.tryGet(current);
            if (!edge || !m_edges.isValid(edge->pair)) break;

            const Edge* pair = m_edges.tryGet(edge->pair);
            if (!pair || !m_edges.isValid(pair->next)) break;

            current = pair->next;

        } while (!(current == first));
    }

    // Fallback: scan all edges.
    for (EdgeHandle edgeHandle : m_edges.getActiveHandles()) {
        if (std::find(excluded.begin(), excluded.end(), edgeHandle) != excluded.end()) continue;
        if (getEdgeOrigin(edgeHandle) == handle) return edgeHandle;
    }

    return INVALID_EDGE;
}

EdgeHandle MeshData::findEdge(VertexHandle origin, VertexHandle tip) const {
    const Vertex* vertex = m_vertices.tryGet(origin);
    if (!vertex || !m_vertices.isValid(tip)) return INVALID_EDGE;

    // First try local topology traversal.
    if (m_edges.isValid(vertex->edge)) {
        EdgeHandle first = vertex->edge;
        EdgeHandle current = first;

        do {
            const Edge* edge = m_edges.tryGet(current);
            if (!edge) break;

            if (edge->tip == tip) return current;

            if (!m_edges.isValid(edge->pair)) break;

            const Edge* pair = m_edges.tryGet(edge->pair);
            if (!pair || !m_edges.isValid(pair->next)) break;

            current = pair->next;

        } while (!(current == first));
    }

    // Fallback in case the local traversal was interrupted
    // by a boundary or incomplete topology.
    for (EdgeHandle edgeHandle : m_edges.getActiveHandles()) {
        const Edge* edge = m_edges.tryGet(edgeHandle);

        if (!edge || !(edge->tip == tip)) continue;
        if (getEdgeOrigin(edgeHandle) == origin) return edgeHandle;
    }

    return INVALID_EDGE;
}

EdgeHandle MeshData::findEdgeInFace(FaceHandle face, VertexHandle origin, VertexHandle tip) const {
    if (!m_faces.isValid(face) ||
        !m_vertices.isValid(origin) ||
        !m_vertices.isValid(tip)) {
        return INVALID_EDGE;
    }

    const Face* faceData = m_faces.tryGet(face);

    if (!faceData || !m_edges.isValid(faceData->edge)) return INVALID_EDGE;

    EdgeHandle first = faceData->edge;
    EdgeHandle current = first;

    do {
        const Edge* edge = m_edges.tryGet(current);

        if (!edge) return INVALID_EDGE;

        if (edge->tip == tip &&
            getEdgeOrigin(current) == origin) {
            return current;
        }

        if (!m_edges.isValid(edge->next)) return INVALID_EDGE;

        current = edge->next;

    } while (!(current == first));

    return INVALID_EDGE;
}

std::vector<VertexHandle> MeshData::getVertexNeighbors(VertexHandle handle) const {
    if (!isValidHandle(handle)) return {};
    const Vertex& vert = m_vertices.get(handle);
    
    if (!isValidHandle(vert.edge)) return {};
    const EdgeHandle start = vert.edge;
    EdgeHandle current = start;
    
    std::vector<VertexHandle> neighbors;
    do {
        if (!isValidHandle(current)) return {};
        const Edge& edge = m_edges.get(current);

        if (isValidHandle(edge.tip)) neighbors.push_back(edge.tip);

        if (!isValidHandle(edge.pair)) return {};
        const Edge& pair = m_edges.get(edge.pair);

        current = pair.next;
    } while (current != start);

    return neighbors;
}
