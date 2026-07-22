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

void MeshData::positionVertex(u32 vIndex, Vec3 position) {
    m_vertices[vIndex].position = position;
}

void MeshData::translateVertex(u32 vIndex, Vec3 delta) {
    m_vertices[vIndex].position += delta;
}