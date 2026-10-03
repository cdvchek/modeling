#include "scene/mesh/mesh_data.hpp"

VertexHandle MeshData::addVertex(Vec3 position) {
    return m_vertices.insert(Vertex{ position });
}

EdgeHandle MeshData::addEdgePair(VertexHandle origin, VertexHandle tip) {
    // Insert both before taking references; inserting can reallocate.
    const EdgeHandle forward = m_edges.insert(Edge{});
    const EdgeHandle backward = m_edges.insert(Edge{});

    Edge& forwardEdge = m_edges.get(forward);
    forwardEdge.pair = backward;
    forwardEdge.tip = tip;

    Edge& backwardEdge = m_edges.get(backward);
    backwardEdge.pair = forward;
    backwardEdge.tip = origin;

    return forward;
}

void MeshData::deleteEdgePair(EdgeHandle handle) {
    const Edge* edge = m_edges.tryGet(handle);
    if (!edge) return;

    const EdgeHandle pair = edge->pair;

    m_edges.remove(handle);
    m_edges.remove(pair);
}

void MeshData::link(EdgeHandle a, EdgeHandle b) {
    m_edges.get(a).next = b;
    m_edges.get(b).prev = a;
}

void MeshData::spliceOut(EdgeHandle handle) {
    const Edge& edge = m_edges.get(handle);
    const EdgeHandle prev = edge.prev;
    const EdgeHandle next = edge.next;
    const FaceHandle face = edge.face;

    link(prev, next);

    Face* faceData = m_faces.tryGet(face);
    if (faceData && faceData->edge == handle) faceData->edge = next;
}

void MeshData::assignFace(EdgeHandle start, FaceHandle face) {
    for (EdgeHandle handle : getLoopEdges(start)) {
        m_edges.get(handle).face = face;
    }

    Face* faceData = m_faces.tryGet(face);
    if (faceData) {
        faceData->edge = start;
        faceData->triangulationDirty = true;
    }
}

void MeshData::repairVertexEdge(VertexHandle handle) {
    Vertex* vertex = m_vertices.tryGet(handle);
    if (!vertex) return;

    if (getEdgeOrigin(vertex->edge) == handle) return;

    vertex->edge = INVALID_EDGE;

    for (EdgeHandle edgeHandle : m_edges.getActiveHandles()) {
        if (getEdgeOrigin(edgeHandle) == handle) {
            vertex->edge = edgeHandle;
            return;
        }
    }
}

void MeshData::repairFaceEdge(FaceHandle handle) {
    Face* face = m_faces.tryGet(handle);
    if (!face) return;

    const Edge* current = m_edges.tryGet(face->edge);
    if (current && current->face == handle) return;

    face->edge = INVALID_EDGE;

    for (EdgeHandle edgeHandle : m_edges.getActiveHandles()) {
        if (m_edges.get(edgeHandle).face == handle) {
            face->edge = edgeHandle;
            return;
        }
    }
}

void MeshData::retargetIncoming(VertexHandle from, VertexHandle to) {
    // Scans every edge so it works while a fan is half-rewired.
    for (EdgeHandle handle : m_edges.getActiveHandles()) {
        Edge& edge = m_edges.get(handle);
        if (edge.tip == from) edge.tip = to;
    }
}
