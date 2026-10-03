#include "scene/mesh/mesh_data.hpp"

void MeshData::deleteFaceWithHalfEdgeLoop(FaceHandle handle) {
    if (!m_faces.isValid(handle)) return;

    std::vector<EdgeHandle> edges = getFaceEdges(handle);

    if (edges.empty()) return;

    for (EdgeHandle edgeHandle : edges) {
        VertexHandle originHandle = getEdgeOrigin(edgeHandle);
        Vertex* origin = m_vertices.tryGet(originHandle);

        if (!origin) continue;

        if (origin->edge == edgeHandle) {
            origin->edge = findOutgoingEdge(
                originHandle,
                edges
            );
        }
    }

    for (EdgeHandle edgeHandle : edges) {
        deleteHalfEdge(edgeHandle);
    }

    deleteFace(handle);
}

void MeshData::deleteHalfEdge(EdgeHandle handle) {
    Edge* edge = m_edges.tryGet(handle);

    if (!edge) {
        return;
    }

    EdgeHandle pairHandle = edge->pair;

    if (m_edges.isValid(pairHandle)) {
        Edge* pair = m_edges.tryGet(pairHandle);

        if (pair && pair->pair == handle) {
            pair->pair = INVALID_EDGE;
        }
    }

    m_edges.remove(handle);
}

void MeshData::deleteFace(FaceHandle handle) {
    m_faces.remove(handle);
}

VertexHandle MeshData::duplicateVertex(VertexHandle handle) {
    Vertex* oldVert = m_vertices.tryGet(handle);
    if (!oldVert) return INVALID_VERTEX;

    Vertex vertex;
    vertex.position = oldVert->position;

    return m_vertices.insert(vertex);
}

FaceHandle MeshData::addQuad(VertexHandle v0, VertexHandle v1, VertexHandle v2, VertexHandle v3) {
    return addFace({ v0, v1, v2, v3 });
}

FaceHandle MeshData::addFace(const std::vector<VertexHandle>& verts) {
    if (verts.size() < 3) return INVALID_FACE;

    for (VertexHandle vert : verts) {
        if (!m_vertices.isValid(vert)) return INVALID_FACE;
    }

    Face face;
    face.edge = INVALID_EDGE;

    FaceHandle faceHandle = m_faces.insert(face);

    std::vector<EdgeHandle> edges;
    edges.reserve(verts.size());

    // Create one half-edge for each side of the face.
    for (std::size_t i = 0; i < verts.size(); ++i) {
        std::size_t next = (i + 1) % verts.size();

        Edge edge;
        edge.tip = verts[next];
        edge.pair = INVALID_EDGE;
        edge.next = INVALID_EDGE;
        edge.prev = INVALID_EDGE;
        edge.face = faceHandle;

        edges.push_back(m_edges.insert(edge));
    }

    // Link the half-edges into a closed loop.
    for (std::size_t i = 0; i < edges.size(); ++i) {
        std::size_t next = (i + 1) % edges.size();
        std::size_t prev = (i + edges.size() - 1) % edges.size();

        Edge* edge = m_edges.tryGet(edges[i]);
        if (!edge) continue;

        edge->next = edges[next];
        edge->prev = edges[prev];
    }

    // Set the face's representative edge.
    Face* newFace = m_faces.tryGet(faceHandle);
    if (newFace) newFace->edge = edges[0];

    // Give each vertex a representative outgoing edge if it doesn't already have one.
    for (std::size_t i = 0; i < verts.size(); ++i) {
        Vertex* vertex = m_vertices.tryGet(verts[i]);
        if (vertex && !m_edges.isValid(vertex->edge)) vertex->edge = edges[i];
    }

    return faceHandle;
}

void MeshData::pairEdges(EdgeHandle a, EdgeHandle b) {
    Edge* edgeA = m_edges.tryGet(a);
    Edge* edgeB = m_edges.tryGet(b);

    if (!edgeA || !edgeB) return;
    if (a == b) return;

    VertexHandle originA = getEdgeOrigin(a);
    VertexHandle originB = getEdgeOrigin(b);

    if (!m_vertices.isValid(originA) || !m_vertices.isValid(originB)) return;
    if (!(originA == edgeB->tip) || !(originB == edgeA->tip)) return;

    edgeA->pair = b;
    edgeB->pair = a;
}
