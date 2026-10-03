#include "scene/mesh/mesh_data.hpp"

VertexHandle MeshData::dissolveEdge(EdgeHandle handle) {
    if (!isValidHandle(handle) || !canCollapseEdge(handle)) return INVALID_VERTEX;

    const VertexHandle origin = getEdgeOrigin(handle);
    const VertexHandle tip = getEdgeTip(handle);

    collapseEdge(handle, (getVertexPosition(origin) + getVertexPosition(tip)) / 2.0f);

    return origin;
}

VertexHandle MeshData::dissolveFace(FaceHandle handle) {
    const std::vector<VertexHandle> corners = getFaceVertices(handle);
    if (corners.size() < 3) return INVALID_VERTEX;

    Vec3 center(0.0f);
    for (VertexHandle corner : corners) center += getVertexPosition(corner);
    center /= static_cast<f32>(corners.size());

    // A collapse can be refused partway through; restore the mesh if it is.
    const MeshArray<Vertex, VertexHandle> savedVertices = m_vertices;
    const MeshArray<Edge, EdgeHandle> savedEdges = m_edges;
    const MeshArray<Face, FaceHandle> savedFaces = m_faces;

    // Merge each corner into the first one, walking around the face.
    // After merging corner i, the next corner is adjacent to the survivor.
    const VertexHandle survivor = corners[0];
    const Vec3 survivorPos = getVertexPosition(survivor);

    for (u32 i = 1; i < static_cast<u32>(corners.size()); ++i) {
        const EdgeHandle edge = findEdge(survivor, corners[i]);

        if (!isValidHandle(edge) || !canCollapseEdge(edge)) {
            m_vertices = savedVertices;
            m_edges = savedEdges;
            m_faces = savedFaces;
            return INVALID_VERTEX;
        }

        collapseEdge(edge, survivorPos);
    }

    m_vertices.get(survivor).position = center;
    setFacesDirtyByVertex(survivor);

    return survivor;
}

bool MeshData::joinFaces(EdgeHandle handle) {
    const Edge* edge = m_edges.tryGet(handle);
    if (!edge) return false;

    const Edge& pair = m_edges.get(edge->pair);

    const FaceHandle keep = edge->face;
    const FaceHandle merged = pair.face;

    if (!isValidHandle(keep) || !isValidHandle(merged) || keep == merged) return false;

    const VertexHandle origin = pair.tip;
    const VertexHandle tip = edge->tip;
    const EdgeHandle start = edge->next;

    // Splice both half-edges out, joining the two face loops into one.
    link(edge->prev, pair.next);
    link(pair.prev, edge->next);

    m_faces.remove(merged);
    assignFace(start, keep);

    deleteEdgePair(handle);

    repairVertexEdge(origin);
    repairVertexEdge(tip);

    return true;
}
