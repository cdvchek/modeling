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

Vec3 MeshData::getVertexPosition(VertexHandle handle) const {
    const Vertex* vert = m_vertices.tryGet(handle);
    if (vert) return vert->position;
    return Vec3();
}

Vec3 MeshData::getFaceNormal(FaceHandle handle) const {
    if (!m_faces.isValid(handle)) return Vec3(0.0f);
    const Face& face = m_faces.get(handle);

    if (!m_edges.isValid(face.edge)) return Vec3(0.0f);

    const EdgeHandle startEdge = face.edge;
    EdgeHandle currentEdge = startEdge;

    Vec3 normal(0.0f);

    do {
        if (!m_edges.isValid(currentEdge)) return Vec3(0.0f);
        const Edge& edge = m_edges.get(currentEdge);

        if (!m_edges.isValid(edge.next)) return Vec3(0.0f);
        const Edge& nextEdge = m_edges.get(edge.next);

        if (!m_vertices.isValid(edge.tip) || !m_vertices.isValid(nextEdge.tip)) return Vec3(0.0f);

        const Vec3& current = m_vertices.get(edge.tip).position;
        const Vec3& next = m_vertices.get(nextEdge.tip).position;

        // Newell's method
        normal.x +=
            (current.y - next.y) *
            (current.z + next.z);

        normal.y +=
            (current.z - next.z) *
            (current.x + next.x);

        normal.z +=
            (current.x - next.x) *
            (current.y + next.y);

        currentEdge = edge.next;

    } while (!(currentEdge == startEdge));

    f32 lengthSq = Vec3::dot(normal, normal);
    if (lengthSq < Math::EPSILON * Math::EPSILON) return Vec3(0.0f);

    return normal / std::sqrt(lengthSq);
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

        // Border edges have no face; skip them but keep circulating.
        if (m_faces.isValid(edge.face)) {
            const Face& face = m_faces.get(edge.face);
            face.triangulationDirty = true;
        }

        if (!m_edges.isValid(edge.pair)) return;
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

FaceHandle MeshData::insertFaceRing(FaceHandle handle) {
    // 1. Get the face's vertices in order.
    std::vector<VertexHandle> oldVerts = getFaceVertices(handle);

    if (oldVerts.size() < 3) {
        return INVALID_FACE;
    }

    // 2. Delete face with half-edge loop.
    deleteFaceWithHalfEdgeLoop(handle);

    // 3. Duplicate those vertices.
    std::vector<VertexHandle> newVerts;

    newVerts.reserve(oldVerts.size());

    for (VertexHandle vertHandle : oldVerts) {
        VertexHandle newVert = duplicateVertex(vertHandle);

        if (!m_vertices.isValid(newVert)) {
            return INVALID_FACE;
        }

        newVerts.push_back(newVert);
    }

    // 4. Create side faces connecting old vertices to new vertices.
    std::vector<FaceHandle> newFaces;

    newFaces.reserve(oldVerts.size());

    for (u32 i = 0; i < static_cast<u32>(oldVerts.size()); ++i) {
        u32 next = (i + 1) % static_cast<u32>(oldVerts.size());

        VertexHandle firstBottom = oldVerts[i];
        VertexHandle secondBottom = oldVerts[next];

        VertexHandle firstTop = newVerts[i];
        VertexHandle secondTop = newVerts[next];

        newFaces.push_back(
            addQuad(
                firstBottom,
                secondBottom,
                secondTop,
                firstTop
            )
        );
    }

    // 5. Pair neighboring side faces along their vertical edges.
    for (u32 i = 0; i < static_cast<u32>(newFaces.size()); ++i) {
        u32 next = (i + 1) % static_cast<u32>(newFaces.size());

        EdgeHandle a = findEdgeInFace(
            newFaces[i],
            oldVerts[next],
            newVerts[next]
        );

        EdgeHandle b = findEdgeInFace(
            newFaces[next],
            newVerts[next],
            oldVerts[next]
        );

        pairEdges(a, b);
    }

    // 6. Pair the bottom edges back into the existing mesh.
    for (u32 i = 0; i < static_cast<u32>(oldVerts.size()); ++i) {
        u32 next = (i + 1) % static_cast<u32>(oldVerts.size());

        VertexHandle origin = oldVerts[i];
        VertexHandle tip = oldVerts[next];

        EdgeHandle existing = findEdge(tip, origin);

        EdgeHandle side = findEdgeInFace(
            newFaces[i],
            origin,
            tip
        );

        pairEdges(existing, side);
    }

    // 7. Create the top face.
    FaceHandle top = addFace(newVerts);

    if (!m_faces.isValid(top)) {
        return INVALID_FACE;
    }

    // 8. Pair top face to side faces.
    for (u32 i = 0; i < static_cast<u32>(newVerts.size()); ++i) {
        u32 next = (i + 1) % static_cast<u32>(newVerts.size());

        EdgeHandle topEdge = findEdgeInFace(
            top,
            newVerts[i],
            newVerts[next]
        );

        EdgeHandle sideEdge = findEdgeInFace(
            newFaces[i],
            newVerts[next],
            newVerts[i]
        );

        pairEdges(topEdge, sideEdge);
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

    if (!m_edges.isValid(face.edge)) return {};
    const EdgeHandle startEdge = face.edge;
    EdgeHandle currentEdge = startEdge;

    // Get the face normal
    Vec3 normal = getFaceNormal(handle);

    f32 lengthSq = Vec3::dot(normal, normal);
    if (lengthSq < Math::EPSILON * Math::EPSILON) return {};

    // Find the dominant axis from the normal and drop that axis
    // from each vertex in the face.
    std::vector<EarVertex> projVerts;

    u8 dominant = 0;

    Vec3 domNormal(
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

    // Project all vertices onto the dominant plane.
    do {
        if (!m_edges.isValid(currentEdge)) return {};
        const Edge& edge = m_edges.get(currentEdge);

        if (!m_vertices.isValid(edge.tip)) return {};
        const Vertex& current = m_vertices.get(edge.tip);

        Vec2 earPosition;

        switch (dominant) {
            case 0:
                // Drop X -> YZ
                earPosition = Vec2(
                    current.position.y,
                    current.position.z
                );
                break;

            case 1:
                // Drop Y -> XZ
                earPosition = Vec2(
                    current.position.x,
                    current.position.z
                );
                break;

            case 2:
                // Drop Z -> XY
                earPosition = Vec2(
                    current.position.x,
                    current.position.y
                );
                break;
        }

        projVerts.push_back(
            EarVertex{
                earPosition,
                edge.tip
            }
        );

        if (!m_edges.isValid(edge.next)) return {};
        currentEdge = edge.next;

    } while (!(currentEdge == startEdge));

    // Run ear clipping on the projected vertices.
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

EdgeHandle MeshData::findEdge(VertexHandle origin, VertexHandle tip) const {
    const Vertex* vertex = m_vertices.tryGet(origin);
    if (!vertex || !m_vertices.isValid(tip)) return INVALID_EDGE;

    // First try local topology traversal.
    if (m_edges.isValid(vertex->edge)) {
        EdgeHandle first = vertex->edge;
        EdgeHandle current = first;

        do {
            const Edge* edge = m_edges.tryGet(current);
            if (!edge) break;

            if (edge->tip == tip) return current;

            if (!m_edges.isValid(edge->pair)) break;

            const Edge* pair = m_edges.tryGet(edge->pair);
            if (!pair || !m_edges.isValid(pair->next)) break;

            current = pair->next;

        } while (!(current == first));
    }

    // Fallback in case the local traversal was interrupted
    // by a boundary or incomplete topology.
    for (EdgeHandle edgeHandle : m_edges.getActiveHandles()) {
        const Edge* edge = m_edges.tryGet(edgeHandle);

        if (!edge || !(edge->tip == tip)) continue;
        if (getEdgeOrigin(edgeHandle) == origin) return edgeHandle;
    }

    return INVALID_EDGE;
}

EdgeHandle MeshData::findEdgeInFace(FaceHandle face, VertexHandle origin, VertexHandle tip) const {
    if (!m_faces.isValid(face) ||
        !m_vertices.isValid(origin) ||
        !m_vertices.isValid(tip)) {
        return INVALID_EDGE;
    }

    const Face* faceData = m_faces.tryGet(face);

    if (!faceData || !m_edges.isValid(faceData->edge)) return INVALID_EDGE;

    EdgeHandle first = faceData->edge;
    EdgeHandle current = first;

    do {
        const Edge* edge = m_edges.tryGet(current);

        if (!edge) return INVALID_EDGE;

        if (edge->tip == tip &&
            getEdgeOrigin(current) == origin) {
            return current;
        }

        if (!m_edges.isValid(edge->next)) return INVALID_EDGE;

        current = edge->next;

    } while (!(current == first));

    return INVALID_EDGE;
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

VertexHandle MeshData::splitEdge(EdgeHandle handle) {
    if (!isValidHandle(handle)) return INVALID_VERTEX;

    Edge& edge = m_edges.get(handle);
    if (!isValidHandle(edge.pair)) return INVALID_VERTEX;

    Edge& pair = m_edges.get(edge.pair);

    if (!isValidHandle(edge.tip)) return INVALID_VERTEX;
    if (!isValidHandle(pair.tip)) return INVALID_VERTEX;

    Edge* oldNextEdge = m_edges.tryGet(edge.next);
    Edge* oldPrevPair = m_edges.tryGet(pair.prev);

    if (!oldNextEdge || !oldPrevPair) return INVALID_VERTEX;

    Vertex& edgeTip = m_vertices.get(edge.tip);
    Vertex& pairTip = m_vertices.get(pair.tip);
    Vec3 firstPos = edgeTip.position;
    Vec3 secondPos = pairTip.position;
    
    Vec3 newVertPos = (firstPos + secondPos) / 2;
    
    VertexHandle newVert = m_vertices.insert({ newVertPos });
    EdgeHandle prevPairH = m_edges.insert({});
    EdgeHandle nextEdgeH = m_edges.insert({});

    // Make sure references and pointers are valid after inserting.
    Edge& prevPair = m_edges.get(prevPairH);
    Edge& nextEdge = m_edges.get(nextEdgeH);
    edge = m_edges.get(handle);
    pair = m_edges.get(edge.pair);
    oldNextEdge = m_edges.tryGet(edge.next);
    oldPrevPair = m_edges.tryGet(pair.prev);

    oldNextEdge->prev = nextEdgeH;
    oldPrevPair->next = prevPairH;

    nextEdge.pair = prevPairH;
    nextEdge.next = edge.next;
    nextEdge.prev = handle;
    nextEdge.tip = edge.tip;
    nextEdge.face = edge.face;

    prevPair.pair = nextEdgeH;
    prevPair.next = edge.pair;
    prevPair.prev = pair.prev;
    prevPair.tip = newVert;
    prevPair.face = pair.face;

    pair.prev = prevPairH;
    edge.tip = newVert;
    edge.next = nextEdgeH;

    m_vertices.get(newVert).edge = edge.pair;

    return newVert;
}

bool MeshData::removeVertex(VertexHandle handle) {
    if (!isValidHandle(handle)) return false;
    const Vertex& vertex = m_vertices.get(handle);

    if (!isValidHandle(vertex.edge)) {
        m_vertices.remove(handle);
        return true;
    }

    const EdgeHandle start = vertex.edge;
    EdgeHandle current = start;

    std::vector<EdgeHandle> deleteEdges;
    do {
        if (!isValidHandle(current)) return false;
        const Edge& currentEdge = m_edges.get(current);

        if (!isValidHandle(currentEdge.pair)) return false;
        const Edge& pair = m_edges.get(currentEdge.pair);

        if (!isValidHandle(pair.next)) return false;
        EdgeHandle next = pair.next;

        if (isValidHandle(currentEdge.face)) removeFace(currentEdge.face);
        deleteEdges.push_back(current);

        current = next;
    } while (current != start);

    for (EdgeHandle edge : deleteEdges) {
        removeEdge(edge);
    }

    m_vertices.remove(handle);
    
    return true;
}

bool MeshData::removeEdge(EdgeHandle handle) {
    if (!isValidHandle(handle)) return false;
    Edge& edge = m_edges.get(handle);

    if (!isValidHandle(edge.pair)) return false;
    EdgeHandle pairHandle = edge.pair;
    Edge& pair = m_edges.get(pairHandle);

    if (!isValidHandle(edge.next) ||
        !isValidHandle(edge.prev) ||
        !isValidHandle(pair.next) ||
        !isValidHandle(pair.prev) ||
        !isValidHandle(edge.tip) ||
        !isValidHandle(pair.tip)) return false;

    // Save everything we need before modifying topology.
    EdgeHandle edgeNextHandle = edge.next;
    EdgeHandle edgePrevHandle = edge.prev;

    EdgeHandle pairNextHandle = pair.next;
    EdgeHandle pairPrevHandle = pair.prev;

    FaceHandle edgeFace = edge.face;
    FaceHandle pairFace = pair.face;

    VertexHandle edgeTipHandle = edge.tip;
    VertexHandle pairTipHandle = pair.tip;

    // Remove faces while their loops are still intact.
    if (isValidHandle(edgeFace)) removeFace(edgeFace);
    if (isValidHandle(pairFace)) removeFace(pairFace);

    // Get references after face removal.
    Edge& edgeNext = m_edges.get(edgeNextHandle);
    Edge& edgePrev = m_edges.get(edgePrevHandle);

    Edge& pairNext = m_edges.get(pairNextHandle);
    Edge& pairPrev = m_edges.get(pairPrevHandle);

    Vertex& edgeTip = m_vertices.get(edgeTipHandle);
    Vertex& pairTip = m_vertices.get(pairTipHandle);

    // Update vertex outgoing edges.
    edgeTip.edge = edgeNextHandle;
    pairTip.edge = pairNextHandle;

    // Splice the two loops together.
    edgeNext.prev = pairPrevHandle;
    pairPrev.next = edgeNextHandle;

    pairNext.prev = edgePrevHandle;
    edgePrev.next = pairNextHandle;

    // Finally remove the two half-edges.
    m_edges.remove(pairHandle);
    m_edges.remove(handle);

    return true;
}

bool MeshData::removeFace(FaceHandle handle) {
    if (!isValidHandle(handle)) return false;
    const Face& face = m_faces.get(handle);

    if (!isValidHandle(face.edge)) return false;
    const EdgeHandle start = face.edge;
    EdgeHandle current = start;
    
    do {
        if (!isValidHandle(current)) return false;
        Edge& currentEdge = m_edges.get(current);
        currentEdge.face = INVALID_FACE;

        current = currentEdge.next;
    } while (current != start);

    m_faces.remove(handle);

    return true;
}

bool MeshData::fillFaceLoop(EdgeHandle handle) {
    if (!isValidHandle(handle)) return false;
    Edge& edge = m_edges.get(handle);

    // If this side already has a face, try the pair.
    if (isValidHandle(edge.face)) {
        if (!isValidHandle(edge.pair)) return false;

        handle = edge.pair;

        if (!isValidHandle(handle)) return false;
        Edge& pair = m_edges.get(handle);

        // Both sides already have faces.
        if (isValidHandle(pair.face)) return false;
    }

    const EdgeHandle start = handle;
    EdgeHandle current = start;

    // Pass 1: validate the border loop
    do {
        if (!isValidHandle(current)) return false;
        const Edge& currentEdge = m_edges.get(current);

        // This edge already belongs to a face.
        if (isValidHandle(currentEdge.face)) return false;
        
        if (!isValidHandle(currentEdge.next)) return false;
        current = currentEdge.next;
    } while (current != start);

    FaceHandle newFace = m_faces.insert(Face{start});

    // Pass 2: assign the face
    current = start;

    do {
        Edge& currentEdge = m_edges.get(current);
        currentEdge.face = newFace;

        current = currentEdge.next;
    } while (current != start);

    return true;
}

bool MeshData::connectVertices(VertexHandle a, VertexHandle b) {
    // 1. Validate vertices
    if (!isValidHandle(a) || !isValidHandle(b)) return false;
    if (a == b) return false;

    const Vertex& aVert = m_vertices.get(a);
    if (!isValidHandle(aVert.edge)) return false;
    
    // 2. Find all face/boundary loops around A and check whether A and B are already connected
    const EdgeHandle start = aVert.edge;
    EdgeHandle current = start;

    std::vector<EdgeHandle> faceLoopStarts;

    do {
        if (!isValidHandle(current)) return false;

        const Edge& currentEdge = m_edges.get(current);

        // Already connected.
        if (currentEdge.tip == b) return false;

        faceLoopStarts.push_back(current);

        if (!isValidHandle(currentEdge.pair)) return false;
        const Edge& pair = m_edges.get(currentEdge.pair);

        if (!isValidHandle(pair.next)) return false;

        current = pair.next;

    } while (current != start);

    // 3. Find a loop around A that also contains B
    bool sameFaceLoop = false;

    EdgeHandle aNextH = INVALID_EDGE;
    EdgeHandle bPrevH = INVALID_EDGE;

    for (const EdgeHandle loopStart : faceLoopStarts) {
        current = loopStart;

        do {
            if (!isValidHandle(current))return false;
            const Edge& currentEdge = m_edges.get(current);

            if (currentEdge.tip == b) {
                sameFaceLoop = true;
                aNextH = loopStart;
                bPrevH = current;

                break;
            }

            if (!isValidHandle(currentEdge.next)) return false;

            current = currentEdge.next;

        } while (current != loopStart);

        if (sameFaceLoop) break;
    }

    if (!sameFaceLoop) return false;

    // 4. Find A-prev/A-next and B-prev/B-next
    if (!isValidHandle(aNextH) || !isValidHandle(bPrevH)) return false;
    const Edge& aNext = m_edges.get(aNextH);
    const Edge& bPrev = m_edges.get(bPrevH);

    if (!isValidHandle(aNext.prev) || !isValidHandle(bPrev.next)) return false;
    const EdgeHandle aPrevH = aNext.prev;
    const EdgeHandle bNextH = bPrev.next;

    if (!isValidHandle(aPrevH) || !isValidHandle(bNextH)) return false;

    // 5. Determine whether we're splitting a face
    const FaceHandle face = aNext.face;
    const bool splittingFace = isValidHandle(face);

    // Both locations must belong to the same loop.
    // If this is a real face, B's edge should have that same face.
    if (splittingFace && bPrev.face != face) return false;

    // 6. Everything required has now been validated.
    // Remove the old face before changing its loop.

    if (splittingFace) if (!removeFace(face)) return false;


    // 7. Create the new paired half-edges
    const EdgeHandle newAH = m_edges.insert(Edge{ INVALID_EDGE, aNextH, bPrevH, a, INVALID_FACE });
    const EdgeHandle newBH = m_edges.insert(Edge{ newAH, bNextH, aPrevH, b, INVALID_FACE });
    m_edges.get(newAH).pair = newBH;

    // 8. Splice the new edges into the topology
    Edge& aPrev = m_edges.get(aPrevH);
    Edge& aNextRef = m_edges.get(aNextH);
    Edge& bPrevRef = m_edges.get(bPrevH);
    Edge& bNext = m_edges.get(bNextH);

    aPrev.next = newBH;
    aNextRef.prev = newAH;

    bPrevRef.next = newAH;
    bNext.prev = newBH;

    // 9. Recreate the two faces if we split a face
    if (splittingFace) {
        if (!fillFaceLoop(newAH)) return false;
        if (!fillFaceLoop(newBH)) return false;
    }

    return true;
}

bool MeshData::mergeVertices(VertexHandle a, VertexHandle b, u8 mergeType) {
    if (!isValidHandle(a) || !isValidHandle(b) || a == b) return false;

    const Vec3 aPos = m_vertices.get(a).position;
    const Vec3 bPos = m_vertices.get(b).position;

    // 1. Collect topology
    EdgeHandle ab;
    EdgeHandle ba;
    EdgeHandle aSurvivingEdge;
    bool foundBetween = false;
    bool foundASurvivingEdge = false;

    {
        const Vertex& aVert = m_vertices.get(a);
        if (!isValidHandle(aVert.edge)) return false;

        const EdgeHandle start = aVert.edge;
        EdgeHandle current = start;

        do {
            if (!isValidHandle(current)) return false;
            const Edge& edge = m_edges.get(current);

            if (edge.tip == b) {
                ab = current;

                if (!isValidHandle(edge.pair)) return false;
                ba = edge.pair;

                foundBetween = true;
            }
            else if (!foundASurvivingEdge) {
                aSurvivingEdge = current;
                foundASurvivingEdge = true;
            }

            if (!isValidHandle(edge.pair)) return false;
            const Edge& pair = m_edges.get(edge.pair);

            if (!isValidHandle(pair.next)) return false;
            current = pair.next;

        } while (current != start);
    }

    if (!foundBetween) return false;

    const std::vector<VertexHandle> aNeighbors = getVertexNeighbors(a);
    const std::vector<VertexHandle> bNeighbors = getVertexNeighbors(b);

    std::vector<VertexHandle> intersection;

    for (VertexHandle aNeighbor : aNeighbors) {
        if (aNeighbor == b) continue;

        for (VertexHandle bNeighbor : bNeighbors) {
            if (bNeighbor == a) continue;

            if (aNeighbor == bNeighbor) {
                intersection.push_back(aNeighbor);
                break;
            }
        }
    }

    // 2. Remove collapsing faces
    for (VertexHandle neighbor : intersection) {
        EdgeHandle bNeighborEdge;
        EdgeHandle aNeighborEdge;

        bool foundBNeighbor = false;
        bool foundANeighbor = false;

        {
            const Vertex& bVert = m_vertices.get(b);
            if (!isValidHandle(bVert.edge)) return false;

            const EdgeHandle start = bVert.edge;
            EdgeHandle current = start;

            do {
                if (!isValidHandle(current)) return false;
                const Edge& edge = m_edges.get(current);

                if (edge.tip == neighbor) {
                    bNeighborEdge = current;
                    foundBNeighbor = true;
                    break;
                }

                if (!isValidHandle(edge.pair)) return false;
                const Edge& pair = m_edges.get(edge.pair);

                if (!isValidHandle(pair.next)) return false;
                current = pair.next;

            } while (current != start);
        }

        {
            const Vertex& aVert = m_vertices.get(a);
            if (!isValidHandle(aVert.edge)) return false;

            const EdgeHandle start = aVert.edge;
            EdgeHandle current = start;

            do {
                if (!isValidHandle(current)) return false;
                const Edge& edge = m_edges.get(current);

                if (edge.tip == neighbor) {
                    aNeighborEdge = current;
                    foundANeighbor = true;
                    break;
                }

                if (!isValidHandle(edge.pair)) return false;
                const Edge& pair = m_edges.get(edge.pair);

                if (!isValidHandle(pair.next)) return false;
                current = pair.next;

            } while (current != start);
        }

        if (!foundBNeighbor || !foundANeighbor) return false;

        if (!removeEdge(bNeighborEdge)) return false;

        if (!isValidHandle(aNeighborEdge)) return false;
        if (!fillFaceLoop(aNeighborEdge)) return false;
    }

    // 3. Redirect surviving B incoming edges to A
    {
        const Vertex& bVert = m_vertices.get(b);
        if (!isValidHandle(bVert.edge)) return false;

        const EdgeHandle start = bVert.edge;
        EdgeHandle current = start;

        std::vector<EdgeHandle> incoming;

        do {
            if (!isValidHandle(current)) return false;
            const Edge& edge = m_edges.get(current);

            if (!isValidHandle(edge.pair)) return false;

            if (edge.pair != ab) {
                incoming.push_back(edge.pair);

                if (!foundASurvivingEdge) {
                    aSurvivingEdge = current;
                    foundASurvivingEdge = true;
                }
            }

            const Edge& pair = m_edges.get(edge.pair);

            if (!isValidHandle(pair.next)) return false;
            current = pair.next;

        } while (current != start);

        for (EdgeHandle incomingHandle : incoming) {
            if (!isValidHandle(incomingHandle)) return false;

            Edge& edge = m_edges.get(incomingHandle);

            if (edge.tip == b)
                edge.tip = a;
        }
    }

    // 4. Remove AB / BA
    if (!isValidHandle(ab) || !isValidHandle(ba)) return false;

    const Edge& abEdge = m_edges.get(ab);
    const Edge& baEdge = m_edges.get(ba);

    if (!isValidHandle(abEdge.prev) ||
        !isValidHandle(abEdge.next) ||
        !isValidHandle(baEdge.prev) ||
        !isValidHandle(baEdge.next)) {
        return false;
    }

    const EdgeHandle abPrev = abEdge.prev;
    const EdgeHandle abNext = abEdge.next;
    const EdgeHandle baPrev = baEdge.prev;
    const EdgeHandle baNext = baEdge.next;

    const FaceHandle abFace = abEdge.face;
    const FaceHandle baFace = baEdge.face;

    m_edges.get(abPrev).next = abNext;
    m_edges.get(abNext).prev = abPrev;

    m_edges.get(baPrev).next = baNext;
    m_edges.get(baNext).prev = baPrev;

    if (isValidHandle(abFace)) {
        Face& face = m_faces.get(abFace);
        if (face.edge == ab)
            face.edge = abNext;
    }

    if (isValidHandle(baFace)) {
        Face& face = m_faces.get(baFace);
        if (face.edge == ba)
            face.edge = baNext;
    }

    m_edges.remove(ab);
    m_edges.remove(ba);

    // 5. Remove B
    m_vertices.remove(b);

    // 6. Repair A.edge
    Vertex& aVert = m_vertices.get(a);

    if (foundASurvivingEdge && isValidHandle(aSurvivingEdge))
        aVert.edge = aSurvivingEdge;
    else
        aVert.edge = EdgeHandle{};

    // 7. Set A position
    if (mergeType == 0)
        aVert.position = (aPos + bPos) / 2.0f;
    else if (mergeType == 1)
        aVert.position = aPos;
    else if (mergeType == 2)
        aVert.position = bPos;
    else
        aVert.position = (aPos + bPos) / 2.0f;

    // Faces around A now include B's old faces and A may have moved,
    // so their cached triangulations are stale.
    setFacesDirtyByVertex(a);

    return true;
}

std::vector<VertexHandle> MeshData::getVertexNeighbors(VertexHandle handle) const {
    if (!isValidHandle(handle)) return {};
    const Vertex& vert = m_vertices.get(handle);
    
    if (!isValidHandle(vert.edge)) return {};
    const EdgeHandle start = vert.edge;
    EdgeHandle current = start;
    
    std::vector<VertexHandle> neighbors;
    do {
        if (!isValidHandle(current)) return {};
        const Edge& edge = m_edges.get(current);

        if (isValidHandle(edge.tip)) neighbors.push_back(edge.tip);

        if (!isValidHandle(edge.pair)) return {};
        const Edge& pair = m_edges.get(edge.pair);

        current = pair.next;
    } while (current != start);

    return neighbors;
}
