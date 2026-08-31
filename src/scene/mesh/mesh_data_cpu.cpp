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

Vec3 MeshData::getVertexPosition(VertexHandle handle) const {
    const Vertex* vert = m_vertices.tryGet(handle);
    if (vert) return vert->position;
    return Vec3();
}

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

const std::vector<Triangle>& MeshData::getFaceTriangles(FaceHandle handle) const {
    static const std::vector<Triangle> emptyTriangles;
    if (!m_faces.isValid(handle)) return emptyTriangles;
    const Face& face = m_faces.get(handle);

    if (face.triangulationDirty) {
        face.triangles = triangulateFace(handle);
        face.triangulationDirty = false;
    }

    return face.triangles;
}

void MeshData::setFacesDirtyByVertex(VertexHandle handle) const {
    if (!m_vertices.isValid(handle)) return;
    const Vertex& vertex = m_vertices.get(handle);

    const EdgeHandle startEdge = vertex.edge;
    EdgeHandle currEdge = vertex.edge;

    do {
        if (!m_edges.isValid(currEdge)) return;
        const Edge& edge = m_edges.get(currEdge);

        if (!m_faces.isValid(edge.face)) return;
        const Face& face = m_faces.get(edge.face);
        face.triangulationDirty = true;

        currEdge = m_edges.get(edge.pair).next;
    } while (!(currEdge == startEdge));
}

void MeshData::setFacesDirtyByEdge(EdgeHandle handle) const {
    if (!m_edges.isValid(handle)) return;
    const Edge& edge = m_edges.get(handle);

    const Edge* prevEdge = m_edges.tryGet(edge.prev);
    if (!prevEdge) return;

    const VertexHandle startVert = prevEdge->tip;
    if (!m_vertices.isValid(startVert)) return;

    const VertexHandle endVert = edge.tip;
    if (!m_vertices.isValid(endVert)) return;

    setFacesDirtyByVertex(startVert);
    setFacesDirtyByVertex(endVert);
}

void MeshData::setFacesDirtyByFace(FaceHandle handle) const {
    if (!m_faces.isValid(handle)) return;
    const Face& face = m_faces.get(handle);
    if (!m_edges.isValid(face.edge)) return;

    const EdgeHandle startEdge = face.edge;
    EdgeHandle currEdge = face.edge;

    do {
        if (!m_edges.isValid(currEdge)) return;
        const Edge& edge = m_edges.get(currEdge);

        if (!m_vertices.isValid(edge.tip)) return;
        setFacesDirtyByVertex(edge.tip);

        currEdge = edge.next;
    } while (!(currEdge == startEdge));
}

void MeshData::positionVertex(VertexHandle handle, Vec3 position) {
    Vertex* vertex = m_vertices.tryGet(handle);
    if (vertex) vertex->position = position;
}

void MeshData::translateVertex(VertexHandle handle, Vec3 delta) {
    Vertex* vertex = m_vertices.tryGet(handle);
    if (vertex) vertex->position += delta;
}

// void MeshData::insertExtrusion(u32 faceIndex) {
//     if (faceIndex >= static_cast<u32>(m_faces.size())) {
//         return;
//     }

//     const u32 faceEdge = m_faces[faceIndex].edge;

//     if (faceEdge == INVALID_INDEX) {
//         return;
//     }

//     std::vector<u32> faceEdges;
//     std::vector<u32> oldVertices;
//     std::vector<u32> oldPairs;

//     u32 edge = faceEdge;

//     do {
//         faceEdges.push_back(edge);
//         oldVertices.push_back(m_edges[edge].tip);
//         oldPairs.push_back(m_edges[edge].pair);

//         edge = m_edges[edge].next;
//     } while (edge != faceEdge);

//     const u32 count = static_cast<u32>(faceEdges.size());

//     if (count < 3) {
//         return;
//     }

//     for (u32 pair : oldPairs) {
//         if (pair == INVALID_INDEX) {
//             return;
//         }
//     }

//     const u32 firstNewVertex =
//         static_cast<u32>(m_vertices.size());

//     const u32 firstNewEdge =
//         static_cast<u32>(m_edges.size());

//     const u32 firstNewFace =
//         static_cast<u32>(m_faces.size());

//     // Prevent unnecessary reallocations.
//     m_vertices.reserve(m_vertices.size() + count);
//     m_edges.reserve(m_edges.size() + count * 4);
//     m_faces.reserve(m_faces.size() + count);

//     std::vector<u32> newVertices;
//     newVertices.reserve(count);

//     for (u32 i = 0; i < count; ++i) {
//         const u32 newVertexIndex =
//             static_cast<u32>(m_vertices.size());

//         m_vertices.push_back(Vertex{
//             m_vertices[oldVertices[i]].position,
//             INVALID_INDEX
//         });

//         newVertices.push_back(newVertexIndex);
//     }

//     for (u32 i = 0; i < count; ++i) {
//         const u32 prev = (i + count - 1) % count;
//         const u32 next = (i + 1) % count;

//         const u32 oldPrev = oldVertices[prev];
//         const u32 oldCurr = oldVertices[i];

//         const u32 newPrev = newVertices[prev];
//         const u32 newCurr = newVertices[i];

//         const u32 selectedEdge = faceEdges[i];
//         const u32 oldPair = oldPairs[i];

//         const u32 sideFace = firstNewFace + i;

//         const u32 s0 = firstNewEdge + i * 4 + 0;

//         const u32 s1 = firstNewEdge + i * 4 + 1;

//         const u32 s2 = firstNewEdge + i * 4 + 2;

//         const u32 s3 = firstNewEdge + i * 4 + 3;

//         const u32 previousSideS3 = firstNewEdge + prev * 4 + 3;

//         const u32 nextSideS1 = firstNewEdge + next * 4 + 1;

//         // Side face.
//         m_faces.push_back(Face{
//             s0,
//             {},
//             true
//         });

//         m_edges.push_back(Edge{
//             selectedEdge, // pair
//             s1,           // next
//             s3,           // prev
//             oldPrev,      // tip
//             sideFace
//         });

//         m_edges.push_back(Edge{
//             previousSideS3,
//             s2,
//             s0,
//             newPrev,
//             sideFace
//         });

//         m_edges.push_back(Edge{
//             oldPair,
//             s3,
//             s1,
//             newCurr,
//             sideFace
//         });

//         m_edges.push_back(Edge{
//             nextSideS1,
//             s0,
//             s2,
//             oldCurr,
//             sideFace
//         });
//     }

//     for (u32 i = 0; i < count; ++i) {
//         const u32 selectedIncomingEdge = faceEdges[i];

//         u32 currEdge = m_edges[m_edges[selectedIncomingEdge].next].pair;

//         while (currEdge != selectedIncomingEdge) {
//             m_edges[currEdge].tip = newVertices[i];

//             const u32 affectedFace = m_edges[currEdge].face;

//             if (affectedFace != INVALID_INDEX) {
//                 m_faces[affectedFace].triangulationDirty = true;
//             }

//             const u32 outgoingEdge = m_edges[currEdge].next;

//             currEdge = m_edges[outgoingEdge].pair;

//             // Defensive check.
//             if (currEdge == INVALID_INDEX) {
//                 break;
//             }
//         }
//     }

//     for (u32 i = 0; i < count; ++i) {
//         const u32 selectedEdge = faceEdges[i];
//         const u32 oldPair = oldPairs[i];

//         const u32 s0 =
//             firstNewEdge + i * 4 + 0;

//         const u32 s2 =
//             firstNewEdge + i * 4 + 2;

//         m_edges[selectedEdge].pair = s0;

//         m_edges[oldPair].pair = s2;
//     }

//     for (u32 i = 0; i < count; ++i) {
//         const u32 next = (i + 1) % count;

//         m_vertices[oldVertices[i]].edge =
//             faceEdges[next];

//         const u32 s3 =
//             firstNewEdge + i * 4 + 3;

//         m_vertices[newVertices[i]].edge = s3;
//     }
// }

struct EarVertex {
    Vec2 position;
    VertexHandle handle;
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
                    prevVert.handle,
                    vert.handle,
                    nextVert.handle
                });
                verts.erase(verts.begin() + i);
                break;
            }
        }

        if (!earFound) return {};
    }

    triangles.push_back(Triangle{
        verts[0].handle,
        verts[1].handle,
        verts[2].handle
    });

    return triangles;
}
    
std::vector<Triangle> MeshData::triangulateFace(FaceHandle handle) const {
    if (!m_faces.isValid(handle)) return {};

    const Face& face = m_faces.get(handle);
    const EdgeHandle startEdge = face.edge;
    EdgeHandle currentEdge = face.edge;

    Vec3 normal = Vec3(0.0f);

    do {
        if (!m_edges.isValid(currentEdge)) return {};
        const Edge& edge = m_edges.get(currentEdge);

        if (!m_edges.isValid(edge.prev) || !m_edges.isValid(edge.next)) return {};
        
        const Edge& prevEdge = m_edges.get(edge.prev);
        const Edge& nextEdge = m_edges.get(edge.next);

        if (!m_vertices.isValid(edge.tip) || !m_vertices.isValid(prevEdge.tip) || !m_vertices.isValid(nextEdge.tip)) return {};

        const Vertex& edgeTip = m_vertices.get(edge.tip);
        const Vertex& prevTip = m_vertices.get(prevEdge.tip);
        const Vertex& nextTip = m_vertices.get(nextEdge.tip);

        Vec3 tipPos = edgeTip.position;
        Vec3 prevTipPos = prevTip.position;
        Vec3 nextTipPos = nextTip.position;

        Vec3 current = tipPos - prevTipPos;
        Vec3 next = nextTipPos - tipPos;

        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);

        currentEdge = edge.next;
    } while (!(currentEdge == startEdge));

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
        if (!m_edges.isValid(currentEdge)) return {};
        const Edge& edge = m_edges.get(currentEdge);

        if (!m_vertices.isValid(edge.tip)) return {};
        Vertex current = m_vertices.get(edge.tip);

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
    } while (!(currentEdge == startEdge));

    // run earclipping on the vector of EarVertex's
    // this should give you what you need to then append the generated triangle indices
    return earclipping(projVerts);
}

VertexHandle MeshData::addVertex(const Vec3& position) {
    return m_vertices.insert(Vertex{position});
}

EdgeHandle MeshData::addEdge(VertexHandle origin, VertexHandle tip) {
    
}
// FaceHandle addFace(const std::vector<VertexHandle>& handles); // Create a face and its half-edge loop from CCW vertices
// FaceHandle addTriangle(VertexHandle v0, VertexHandle v1, VertexHandle v2); // Convenience wrapper for creating a triangle
// FaceHandle addQuad(VertexHandle v0, VertexHandle v1, VertexHandle v2, VertexHandle v3); // Convenience wrapper for creating a quad
// VertexHandle duplicateVertex(VertexHandle handle); // Duplicate a vertex at the same position

// void pairEdges(EdgeHandle a, EdgeHandle b); // Pair two half-edges
// void unpairEdge(EdgeHandle handle); // Disconnect an edge from its pair
// void setEdgeTip(EdgeHandle edge, VertexHandle tip); // Change an edge's tip and dirty the affected face
// void setVertexEdge(VertexHandle vertex, EdgeHandle edge); // Set a representative outgoing half-edge
// void setFaceEdge(FaceHandle face, EdgeHandle edge); // Set a representative boundary edge for a face

// VertexHandle getEdgeOrigin(EdgeHandle handle) const; // Return the starting vertex of the half edge (not the tip)
// std::vector<EdgeHandle> getFaceEdges(FaceHandle handle) const; // Return all half-edges around a face in order
// std::vector<u32> getFaceVertices(u32 face) const; // Return all vertices around a face in order
// std::vector<EdgeHandle> getVertexEdges(VertexHandle handle) const; // Return all half-edges (incoming and outgoing) incident to a vertex
// std::vector<EdgeHandle> getVertexIncomingEdges(VertexHandle handle) const; // Return all half-edges whose tip is the vertex
// std::vector<EdgeHandle> getVertexOutgoingEdges(VertexHandle handle) const; // Return all half-edges originating at the vertex
// std::vector<FaceHandle> getVertexFaces(VertexHandle handle) const; // Return all faces touching a vertex
// std::vector<FaceHandle> getAdjacentFaces(FaceHandle handle) const; // Return all faces sharing an edge with a face
// std::vector<VertexHandle> getAdjacentVertices(VertexHandle handle) const; // Return all vertices directly connected to a vertex
    
// bool isBoundaryEdge(EdgeHandle handle) const; // Return true for any half-edge that is adjacent to only one face
// bool isBoundaryVertex(VertexHandle handle) const; // Return true for any vertex that has an incident boundary edge
// bool areVerticesConnected(VertexHandle a, VertexHandle b) const; // Return true if given vertices are adjacent
// bool areFacesAdjacent(FaceHandle a, FaceHandle b) const; // Return true if given faces are adjacent
// EdgeHandle findEdge(VertexHandle origin, VertexHandle tip) const; // Finds a directed half-edge between two vertices, returns invalid index if no half-edge found

// VertexHandle splitEdge(EdgeHandle handle, const Vec3& position); // Insert a vertex into an existing edge
// VertexHandle splitEdge(EdgeHandle handle, f32 t); // Insert a vertex somewhere along an edge
// void collapseEdge(EdgeHandle handle); // Collapse an edge and merge its vertices
// FaceHandle splitFace(FaceHandle face, VertexHandle vertexA, VertexHandle vertexB); // Connect two vertices of a face, splitting it into two faces
// void mergeFaces(FaceHandle handle); // Remove the shared edge between two faces and combine them
// void detachVertexFromFace(VertexHandle vertex, FaceHandle face); // Duplicate/separate a vertex so the face can move independently
// void detachFace(FaceHandle handle); // Duplicate the necessary boundary topology so the face is disconnected from its neighbors
// void removeFace(FaceHandle handle); // Remove a face while leaving appropriate boundary topology
// void removeEdge(EdgeHandle handle); // Remove an edge/paair while repairing references
// void removeVertex(VertexHandle handle); // Remove an unused vertex