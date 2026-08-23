#include "scene/mesh/mesh_data.hpp"
#include "scene/mesh/mesh_factory.hpp"
#include "core/math/math_utils.hpp"

#include <cmath>

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

Vec3 MeshData::getVertexPosition(u32 vertexIndex) const {
    if (vertexIndex > static_cast<u32>(m_vertices.size())) return Vec3();
    return m_vertices[vertexIndex].position;
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



const std::vector<Triangle>& MeshData::getFaceTriangles(u32 faceIndex) const {
    static const std::vector<Triangle> emptyTriangles;
    if (faceIndex >= m_faces.size()) return emptyTriangles;
    const Face& face = m_faces[faceIndex];

    if (face.triangulationDirty) {
        face.triangles = triangulateFace(faceIndex);
        face.triangulationDirty = false;
    }

    return face.triangles;
}

void MeshData::setFacesDirtyByVertex(u32 vertexInd) const {
    if (vertexInd >= static_cast<u32>(m_vertices.size())) return;
    const Vertex& vertex = m_vertices[vertexInd];

    const u32 startEdge = vertex.edge;
    u32 currEdge = vertex.edge;

    do {
        const Edge& edge = m_edges[currEdge];

        const Face& face = m_faces[edge.face];
        face.triangulationDirty = true;

        currEdge = m_edges[edge.pair].next;
    } while (currEdge != startEdge);
}

void MeshData::setFacesDirtyByEdge(u32 edgeInd) const {
    if (edgeInd >= static_cast<u32>(m_edges.size())) return;
    const Edge& edge = m_edges[edgeInd];

    const u32 startVert = m_edges[edge.prev].tip;
    const u32 endVert = edge.tip;

    setFacesDirtyByVertex(startVert);
    setFacesDirtyByVertex(endVert);
}

void MeshData::setFacesDirtyByFace(u32 faceInd) const {
    if (faceInd >= static_cast<u32>(m_faces.size())) return;
    const Face& face = m_faces[faceInd];

    const u32 startEdge = face.edge;
    u32 currEdge = face.edge;

    do {
        const Edge& edge = m_edges[currEdge];
        setFacesDirtyByVertex(edge.tip);
        currEdge = edge.next;
    } while (currEdge != startEdge);
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

f32 computeSignedArea(const std::vector<EarVertex>& verts) {
    f32 area = 0.0f;

    for (u32 i = 0; i < static_cast<u32>(verts.size()); ++i) {
        const Vec2& current = verts[i].position;
        const Vec2& next = verts[(i + 1) % static_cast<u32>(verts.size())].position;

        area += current.x * next.y - next.x * current.y;
    }

    return area * 0.5f;
}

bool isPointInTriangle(Vec2 tri1, Vec2 tri2, Vec2 tri3, Vec2 point) {
    auto cross = [](const Vec2& a, const Vec2& b, const Vec2& c) {
        Vec2 ab = b - a;
        Vec2 ac = c - a;
        return ab.x * ac.y - ab.y * ac.x;
    };

    f32 d1 = cross(tri1, tri2, point);
    f32 d2 = cross(tri2, tri3, point);
    f32 d3 = cross(tri3, tri1, point);

    bool hasNegative =
        d1 < -Math::EPSILON ||
        d2 < -Math::EPSILON ||
        d3 < -Math::EPSILON;

    bool hasPositive =
        d1 > Math::EPSILON ||
        d2 > Math::EPSILON ||
        d3 > Math::EPSILON;

    return !(hasNegative && hasPositive);
}

std::vector<Triangle> earclipping(std::vector<EarVertex> verts) {
    // this will probably be a nested loop
    // outer loop will just keep running until every vertex has been triangled sort of?
    // inner loop will run over each vertex, test if the vertex is convex and if so, make a triangle
    // and remove that vertex, if the vertex is concave, try the next vertex.

    bool counterClockwise = computeSignedArea(verts) > 0.0f;

    std::vector<Triangle> triangles;
    while (verts.size() > 3) {
        bool earFound = false;
        for (u32 i = 0; i < static_cast<u32>(verts.size()); ++i) {
            u32 prevIndex = (i == 0) ? static_cast<u32>(verts.size()) - 1 : i - 1;
            u32 nextIndex = (i + 1) % static_cast<u32>(verts.size());
            
            const EarVertex& prevVert = verts[prevIndex];
            const EarVertex& vert = verts[i];
            const EarVertex& nextVert = verts[nextIndex];
            
            Vec2 incoming = vert.position - prevVert.position;
            Vec2 outgoing = nextVert.position - vert.position;
            
            f32 cross = incoming.x * outgoing.y - incoming.y * outgoing.x;

            bool isConvex = counterClockwise ? (cross > Math::EPSILON) : (cross < -Math::EPSILON);
            
            if (isConvex) { // convex
                bool hasInsideVert = false;
                for (u32 j = 0; j < static_cast<u32>(verts.size()); ++j) {
                    if (j == prevIndex || j == i || j == nextIndex) continue;
                    const EarVertex& testVert = verts[j];

                    if (isPointInTriangle(
                        prevVert.position,
                        vert.position,
                        nextVert.position,
                        testVert.position
                    )) {
                        hasInsideVert = true;
                        break;
                    }
                }

                if (hasInsideVert) continue;
                
                earFound = true;
                triangles.push_back(Triangle{
                    prevVert.meshVertexIndex,
                    vert.meshVertexIndex,
                    nextVert.meshVertexIndex
                });
                verts.erase(verts.begin() + i);
                break;
            }
        }

        if (!earFound) return {};
    }

    triangles.push_back(Triangle{
        verts[0].meshVertexIndex,
        verts[1].meshVertexIndex,
        verts[2].meshVertexIndex
    });

    return triangles;
}
    
std::vector<Triangle> MeshData::triangulateFace(u32 faceIndex) const {
    if (faceIndex >= m_faces.size()) return {};

    const Face& face = m_faces[faceIndex];
    const u32 startEdge = face.edge;
    u32 currentEdge = face.edge;

    Vec3 normal = Vec3(0.0f);

    do {
        const Edge& edge = m_edges[currentEdge];

        Vec3 tipPos = m_vertices[edge.tip].position;
        Vec3 prevTipPos = m_vertices[m_edges[edge.prev].tip].position;
        Vec3 nextTipPos = m_vertices[m_edges[edge.next].tip].position;

        Vec3 current = tipPos - prevTipPos;
        Vec3 next = nextTipPos - tipPos;

        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);

        currentEdge = edge.next;
    } while (currentEdge != startEdge);

    f32 lengthSq = Vec3::dot(normal, normal);
    if (lengthSq < Math::EPSILON * Math::EPSILON) return {};

    // then find the dominant axis from the normal and drop that axis from each vertex in the face
    // dropping that axis from each vertex projects it into the most parallel plane (XY, XZ, or YZ)
    // while dropping the axis, package the vertices into a vector of EarVertex's (EarVertex is shown above)
    std::vector<EarVertex> projVerts;
    currentEdge = startEdge;

    u8 dominant = 0;
    Vec3 domNormal = Vec3(
        std::abs(normal.x),
        std::abs(normal.y),
        std::abs(normal.z)
    );

    if (domNormal.x >= domNormal.y && domNormal.x >= domNormal.z) {
        dominant = 0;
    } else if (domNormal.y >= domNormal.x && domNormal.y >= domNormal.z) {
        dominant = 1;
    } else {
        dominant = 2;
    }

    do {
        const Edge& edge = m_edges[currentEdge];
        Vertex current = m_vertices[edge.tip];

        Vec2 earPosition;
        switch (dominant) {
        case 0:
            earPosition = Vec2(current.position.y, current.position.z);
            break;

        case 1:
            earPosition = Vec2(current.position.x, current.position.z);
            break;

        case 2:
            earPosition = Vec2(current.position.x, current.position.y);
            break;
        }
        projVerts.push_back(EarVertex{ earPosition, edge.tip });

        currentEdge = edge.next;
    } while (currentEdge != startEdge);

    // run earclipping on the vector of EarVertex's
    // this should give you what you need to then append the generated triangle indices
    return earclipping(projVerts);
}