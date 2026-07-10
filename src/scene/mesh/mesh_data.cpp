#include "scene/mesh/mesh_data.hpp"

const std::vector<Vertex>& MeshData::getVertices() const {
    return m_vertices;
}

void MeshData::setVertices(std::vector<Vertex> verts) {
    m_vertices = verts;
}

u32 MeshData::addVertex(const Vertex& vertex) {
    m_vertices.push_back(vertex);
    return static_cast<u32>(m_vertices.size() - 1);
}

void MeshData::moveVertex(u32 index, const Vec3& newPosition) {
    if (index >= m_vertices.size()) return;

    m_vertices[index].position = newPosition;
}

void MeshData::translateVertex(u32 index, const Vec3& delta) {
    if (index >= m_vertices.size()) return;

    m_vertices[index].position += delta;
}

// const std::vector<u32>& MeshData::getIndices() {
//     if (m_dirty) {
//         generateIndices();
//         m_dirty = false;
//     }

//     return m_indices;
// }

const std::vector<Edge>& MeshData::getEdges() const {
    return m_edges;
}

void MeshData::setEdges(std::vector<Edge> edges) {
    m_edges = edges;
}

u32 MeshData::addEdge(const Edge& edge) {
    m_edges.push_back(edge);
    return static_cast<u32>(m_edges.size() - 1);
}

void MeshData::deleteEdge(u32 edgeIndex) {
    if (edgeIndex >= m_edges.size()) return;
    m_edges.erase(m_edges.begin() + edgeIndex);
}

const std::vector<Face>& MeshData::getFaces() const {
    return m_faces;
}

void MeshData::setFaces(std::vector<Face> faces) {
    m_faces = faces;
}

u32 MeshData::addFace(const Face& face) {
    m_faces.push_back(face);
    return static_cast<u32>(m_faces.size() - 1);
}

void MeshData::deleteFace(u32 faceIndex) {
    if (faceIndex >= m_faces.size()) return;
    m_faces.erase(m_faces.begin() + faceIndex);
}