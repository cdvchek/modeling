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

std::vector<EdgeHandle> MeshData::getEdgeLoop(EdgeHandle start) const {
    if (!m_edges.isValid(start)) return {};

    const EdgeHandle startPair = m_edges.get(start).pair;

    if (isBorder(start)) return getLoopEdges(start);
    if (isBorder(startPair)) return getLoopEdges(startPair);

    std::vector<EdgeHandle> edges = { start };

    auto inLoop = [&](EdgeHandle edge) {
        const EdgeHandle pair = m_edges.get(edge).pair;
        return std::find(edges.begin(), edges.end(), edge) != edges.end() ||
               std::find(edges.begin(), edges.end(), pair) != edges.end();
    };

    // Go straight through each vertex with exactly 4 edges, in both directions.
    for (EdgeHandle current : { start, startPair }) {
        while (true) {
            const VertexHandle vertex = getEdgeTip(current);
            if (getOutgoingEdges(vertex).size() != 4 || isBorderVertex(vertex)) break;

            current = m_edges.get(m_edges.get(m_edges.get(current).next).pair).next;
            if (inLoop(current)) break;

            edges.push_back(current);
        }
    }

    return edges;
}

std::vector<EdgeHandle> MeshData::getEdgeRing(EdgeHandle start) const {
    std::vector<EdgeHandle> edges;
    std::vector<FaceHandle> faces;
    walkRing(start, edges, faces);
    return edges;
}

std::vector<FaceHandle> MeshData::getFaceLoop(EdgeHandle start) const {
    std::vector<EdgeHandle> edges;
    std::vector<FaceHandle> faces;
    walkRing(start, edges, faces);
    return faces;
}

void MeshData::walkRing(EdgeHandle start, std::vector<EdgeHandle>& edges, std::vector<FaceHandle>& faces) const {
    if (!m_edges.isValid(start)) return;

    edges.push_back(start);

    auto inRing = [&](EdgeHandle edge) {
        const EdgeHandle pair = m_edges.get(edge).pair;
        return std::find(edges.begin(), edges.end(), edge) != edges.end() ||
               std::find(edges.begin(), edges.end(), pair) != edges.end();
    };

    // Cross each quad to its opposite edge, in both directions.
    for (EdgeHandle current : { start, m_edges.get(start).pair }) {
        while (true) {
            const FaceHandle face = m_edges.get(current).face;
            if (!m_faces.isValid(face) || getFaceEdges(face).size() != 4) break;
            if (std::find(faces.begin(), faces.end(), face) != faces.end()) break;

            faces.push_back(face);

            const EdgeHandle opposite = m_edges.get(m_edges.get(current).next).next;
            if (inRing(opposite)) break;

            edges.push_back(opposite);
            current = m_edges.get(opposite).pair;
        }
    }
}
