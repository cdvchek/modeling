#include "scene/mesh/mesh_data.hpp"
#include "scene/mesh/mesh_factory.hpp"

void MeshData::setMesh(PresetMesh meshType) {
    PackagedMesh pMesh;

    switch(meshType) {
        case PresetMesh::Cube:
            pMesh = MeshFactory::cube();
            break;
    }

    m_vertices = pMesh.vertices;
    m_edges = pMesh.edges;
    m_faces = pMesh.faces;

    m_dirty = true;
}

const Vertex* MeshData::getVertex(VertexHandle handle) const {
    if (!m_vertices.isValid(handle))
        return nullptr;

    return &m_vertices.get(handle);
}

const Edge* MeshData::getEdge(EdgeHandle handle) const {
    if (!m_edges.isValid(handle))
        return nullptr;

    return &m_edges.get(handle);
}

const Face* MeshData::getFace(FaceHandle handle) const {
    if (!m_faces.isValid(handle))
        return nullptr;

    return &m_faces.get(handle);
}

const std::vector<Vertex> MeshData::getVertices() const {
    return m_vertices.getActiveValues();
}

const std::vector<Face> MeshData::getFaces() const {
    return m_faces.getActiveValues();
}

const std::vector<VertexHandle> MeshData::getVertexHandles() const {
    return m_vertices.getActiveHandles();
}

const std::vector<EdgeHandle> MeshData::getEdgeHandles() const {
    return m_edges.getActiveHandles();
}

const std::vector<FaceHandle> MeshData::getFaceHandles() const {
    return m_faces.getActiveHandles();
}

VertexHandle MeshData::getEdgeOrigin(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    if (!edge) return INVALID_VERTEX;

    const Edge* pair = m_edges.tryGet(edge->pair);
    if (!pair || !m_vertices.isValid(pair->tip)) return INVALID_VERTEX;

    return pair->tip;
}

VertexHandle MeshData::getEdgeTip(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    if (!edge) return INVALID_VERTEX;

    if (m_vertices.isValid(edge->tip)) return edge->tip;
    else return INVALID_VERTEX;
}

bool MeshData::isValidHandle(VertexHandle handle) const {
    return m_vertices.isValid(handle);
}

bool MeshData::isValidHandle(EdgeHandle handle) const {
    return m_edges.isValid(handle);
}

bool MeshData::isValidHandle(FaceHandle handle) const {
    return m_faces.isValid(handle);
}
