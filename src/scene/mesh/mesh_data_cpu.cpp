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

struct EarVertex {
    Vec2 position;
    u32 meshVertexIndex;
};

std::vector<Triangle> MeshData::triangulateFace(u32 faceIndex) const {
    if (faceIndex >= m_faces.size()) return {};

    const Face& face = m_faces[faceIndex];
    const u32 startEdge = face.edge;
    u32 currentEdge = face.edge;

    Vec3 normal = Vec3(0.0f);

    do {
        const Edge& edge = m_edges[currentEdge];
        Vec3 current = edge.tip - m_edges[edge.prev].tip;
        Vec3 next = m_edges[edge.next].tip - edge.tip;

        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);

        currentEdge = edge.next;
    } while (currentEdge != startEdge);

    f32 lengthSq = Vec3::dot(normal, normal);

    // TODO: implement EPSILON as a checker
    // if (lengthSq < EPSILON * EPSILON) return {};
    // then find the dominant axis from the normal and drop that axis from each vertex in the face
    // dropping that axis from each vertex projects it into the most parallel plane (XY, XZ, or YZ)
    // after dropping the axis, package the vertices into a vector of EarVertex's (EarVertex is shown above)
    // run earclipping on the vector of EarVertex's
    // this should give you what you need to then append the generated triangle indices
}