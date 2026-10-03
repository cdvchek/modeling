#include "scene/mesh/mesh_data.hpp"
#include "core/math/math_utils.hpp"

#include <cmath>
#include <algorithm>

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
