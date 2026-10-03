#include "scene/mesh/mesh_data.hpp"

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

std::vector<EdgeHandle> MeshData::getOutgoingEdges(VertexHandle handle) const {
    const Vertex* vertex = m_vertices.tryGet(handle);
    if (!vertex || !m_edges.isValid(vertex->edge)) return {};

    std::vector<EdgeHandle> edges;
    const EdgeHandle start = vertex->edge;
    EdgeHandle current = start;

    do {
        const Edge* edge = m_edges.tryGet(current);
        if (!edge) return {};

        edges.push_back(current);

        const Edge* pair = m_edges.tryGet(edge->pair);
        if (!pair) return {};

        current = pair->next;

        // A broken fan might never return to the start.
        if (edges.size() > m_edges.activeSize()) return {};
    } while (current != start);

    return edges;
}

std::vector<EdgeHandle> MeshData::getIncomingEdges(VertexHandle handle) const {
    std::vector<EdgeHandle> edges = getOutgoingEdges(handle);

    for (EdgeHandle& edge : edges) {
        edge = m_edges.get(edge).pair;
    }

    return edges;
}

std::vector<EdgeHandle> MeshData::getLoopEdges(EdgeHandle start) const {
    if (!m_edges.isValid(start)) return {};

    std::vector<EdgeHandle> edges;
    EdgeHandle current = start;

    do {
        const Edge* edge = m_edges.tryGet(current);
        if (!edge) return {};

        edges.push_back(current);
        current = edge->next;

        if (edges.size() > m_edges.activeSize()) return {};
    } while (current != start);

    return edges;
}

bool MeshData::isBorder(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    return edge && !m_faces.isValid(edge->face);
}

bool MeshData::isBorderVertex(VertexHandle handle) const {
    // Every border loop passing through a vertex leaves it along an
    // outgoing border half-edge, so checking outgoing edges is enough.
    for (EdgeHandle edge : getOutgoingEdges(handle)) {
        if (isBorder(edge)) return true;
    }

    return false;
}

EdgeHandle MeshData::findEdge(VertexHandle origin, VertexHandle tip) const {
    if (!m_vertices.isValid(tip)) return INVALID_EDGE;

    for (EdgeHandle handle : getOutgoingEdges(origin)) {
        if (m_edges.get(handle).tip == tip) return handle;
    }

    return INVALID_EDGE;
}

std::vector<VertexHandle> MeshData::getVertexNeighbors(VertexHandle handle) const {
    std::vector<VertexHandle> neighbors;

    for (EdgeHandle edge : getOutgoingEdges(handle)) {
        neighbors.push_back(m_edges.get(edge).tip);
    }

    return neighbors;
}
