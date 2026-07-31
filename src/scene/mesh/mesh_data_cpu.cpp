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

const std::vector<Vertex> MeshData::getVertices() const {
    return m_vertices;
}

const std::vector<Face> MeshData::getFaces() const {
    return m_faces;
}

std::vector<u32> MeshData::getFaceVertices(u32 faceIndex) const {
    if (faceIndex >= m_faces.size()) {
        return {};
    }

    const u32 firstEdgeIndex = m_faces[faceIndex].edge;

    if (firstEdgeIndex == INVALID_INDEX ||
        firstEdgeIndex >= m_edges.size()) {
        return {};
    }

    std::vector<u32> vertices;
    u32 edgeIndex = firstEdgeIndex;

    do {
        if (edgeIndex == INVALID_INDEX ||
            edgeIndex >= m_edges.size()) {
            return {};
        }

        const Edge& edge = m_edges[edgeIndex];

        if (edge.tip == INVALID_INDEX ||
            edge.tip >= m_vertices.size()) {
            return {};
        }

        vertices.push_back(edge.tip);
        edgeIndex = edge.next;

        // A valid face cannot contain more half-edges than exist globally.
        if (vertices.size() > m_edges.size()) {
            return {};
        }
    } while (edgeIndex != firstEdgeIndex);

    return vertices;
}

std::vector<Triangle> MeshData::getFaceTriangles(u32 faceIndex) {
    if (faceIndex >= m_faces.size()) return {};
    Face& face = m_faces[faceIndex];

    if (face.triangulationDirty) {
        // face.triangles = triangulateFace(faceIndex);
        face.triangulationDirty = false;
    }

    return face.triangles;
}

void MeshData::positionVertex(u32 vIndex, Vec3 position) {
    m_vertices[vIndex].position = position;
}

void MeshData::translateVertex(u32 vIndex, Vec3 delta) {
    m_vertices[vIndex].position += delta;
}