#include "scene/mesh/mesh_data.hpp"
#include "scene/mesh/mesh_factory.hpp"
#include "core/math/math_utils.hpp"

#include <cmath>
#include <algorithm>

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

FaceHandle MeshData::insertExtrusion(FaceHandle handle) {
    // 1. get the face's vertices in order
    std::vector<VertexHandle> oldVerts = getFaceVertices(handle);
    // 2. delete face with half edge loop
    deleteFaceWithHalfEdgeLoop(handle);
    // 3. duplicate those vertices
    std::vector<VertexHandle> newVerts;
    newVerts.reserve(oldVerts.size());
    for (const VertexHandle vertHandle : oldVerts) {
        newVerts.push_back(duplicateVertex(vertHandle));
    }
    // 4. create side faces connecting old vertices to new vertices
    std::vector<FaceHandle> newFaces;
    newFaces.reserve(oldVerts.size());
    for (u32 i = 0; i < static_cast<u32>(oldVerts.size()); ++i) {
        VertexHandle firstTop = newVerts[i];
        VertexHandle secondTop = newVerts[i+1 % newVerts.size()];
        VertexHandle firstBottom = oldVerts[i];
        VertexHandle secondBottom = oldVerts[i+1 % oldVerts.size()];

        newFaces.push_back(addQuad(firstTop, secondTop, secondBottom, firstBottom));
    }
    // 5. pair the new side edges correctly
    for (u32 i = 0; i < static_cast<u32>(oldVerts.size()); ++i) {
        EdgeHandle a = findEdgeInFace(newFaces[i], oldVerts[i+1 % oldVerts.size()], newVerts[i+1 % newVerts.size()]);
        EdgeHandle b = findEdgeInFace(newFaces[i+1 % newFaces.size()], newVerts[i+1 % newVerts.size()], oldVerts[i+1 % oldVerts.size()]);
        pairEdges(a, b);
    }
    // 6. pair the bottom extrusion sides to entire mesh correctly
    for (u32 i = 0; i < static_cast<u32>(oldVerts.size()); ++i) {
        VertexHandle origin = oldVerts[i];
        VertexHandle tip = oldVerts[i+1 % oldVerts.size()];

        EdgeHandle a = findEdge(tip, origin);
        EdgeHandle b = findEdgeInFace(newFaces[i], origin, tip);
        pairEdges(a, b);
    }
    // 7. create top face
    FaceHandle top = addFace(newVerts);
    // 8. pair top face to side faces
    for (u32 i = 0; i < static_cast<u32>(newVerts.size()); ++i) {
        VertexHandle origin = newVerts[i];
        VertexHandle tip = newVerts[i+1 % newVerts.size()];

        EdgeHandle a = findEdgeInFace(top, origin, tip);
        EdgeHandle b = findEdgeInFace(newFaces[i], tip, origin);
        pairEdges(a, b);
    }

    return top;
}

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

VertexHandle MeshData::getEdgeOrigin(EdgeHandle handle) const {
    if (!m_edges.isValid(handle)) return INVALID_VERTEX;

    const Edge& edge = m_edges.get(handle);
    if (!m_edges.isValid(edge.prev)) return INVALID_VERTEX;

    const Edge& prev = m_edges.get(edge.prev);

    if (!(edge.face == prev.face)) return INVALID_VERTEX;
    if (!m_vertices.isValid(prev.tip)) return INVALID_VERTEX;

    return prev.tip;
}

EdgeHandle MeshData::findOutgoingEdge(VertexHandle handle, const std::vector<EdgeHandle>& excluded) const {
    const Vertex* vertex = m_vertices.tryGet(handle);
    if (!vertex) return INVALID_EDGE;

    // First try local half-edge traversal.
    if (m_edges.isValid(vertex->edge)) {
        EdgeHandle first = vertex->edge;
        EdgeHandle current = first;

        do {
            if (std::find(excluded.begin(), excluded.end(), current) == excluded.end()) return current;

            const Edge* edge = m_edges.tryGet(current);
            if (!edge || !m_edges.isValid(edge->pair)) break;

            const Edge* pair = m_edges.tryGet(edge->pair);
            if (!pair || !m_edges.isValid(pair->next)) break;

            current = pair->next;

        } while (!(current == first));
    }

    // Fallback: scan all edges.
    for (EdgeHandle edgeHandle : m_edges.getActiveHandles()) {
        if (std::find(excluded.begin(), excluded.end(), edgeHandle) != excluded.end()) continue;
        if (getEdgeOrigin(edgeHandle) == handle) return edgeHandle;
    }

    return INVALID_EDGE;
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

}

FaceHandle MeshData::addFace(const std::vector<VertexHandle>& verts) {

}

EdgeHandle MeshData::findEdge(VertexHandle origin, VertexHandle tip) const {

}

EdgeHandle MeshData::findEdgeInFace(FaceHandle face, VertexHandle origin, VertexHandle tip) const {

}

void MeshData::pairEdges(EdgeHandle a, EdgeHandle b) {

}