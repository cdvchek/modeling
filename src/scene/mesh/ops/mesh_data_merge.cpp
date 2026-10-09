#include "scene/mesh/mesh_data.hpp"

#include <algorithm>

bool MeshData::mergeVertices(VertexHandle a, VertexHandle b, u8 mergeType) {
    if (!isValidHandle(a) || !isValidHandle(b) || a == b) return false;

    const EdgeHandle ab = findEdge(a, b);
    if (!isValidHandle(ab)) return false;

    if (!canCollapseEdge(ab)) return false;

    const Vec3 aPos = m_vertices.get(a).position;
    const Vec3 bPos = m_vertices.get(b).position;

    Vec3 position = (aPos + bPos) / 2.0f;
    if (mergeType == 1) position = aPos;
    else if (mergeType == 2) position = bPos;

    collapseEdge(ab, position);

    return true;
}

bool MeshData::canCollapseEdge(EdgeHandle ab) const {
    const EdgeHandle ba = m_edges.get(ab).pair;
    const VertexHandle a = getEdgeOrigin(ab);
    const VertexHandle b = getEdgeTip(ab);

    // 1. Find the apex of each side that is a triangle (face or border hole).
    std::vector<VertexHandle> apexes;

    for (EdgeHandle side : { ab, ba }) {
        const std::vector<EdgeHandle> loop = getLoopEdges(side);
        if (loop.size() != 3) continue;

        if (std::find(loop.begin(), loop.end(), m_edges.get(side).pair) != loop.end()) continue;

        apexes.push_back(getEdgeTip(m_edges.get(side).next));
    }

    if (apexes.size() == 2 && apexes[0] == apexes[1]) return false;

    // 2. Every vertex connected to both a and b must be a triangle apex.
    const std::vector<VertexHandle> aNeighbors = getVertexNeighbors(a);
    const std::vector<VertexHandle> bNeighbors = getVertexNeighbors(b);

    for (VertexHandle neighbor : aNeighbors) {
        if (neighbor == b) continue;
        if (std::find(bNeighbors.begin(), bNeighbors.end(), neighbor) == bNeighbors.end()) continue;
        if (std::find(apexes.begin(), apexes.end(), neighbor) == apexes.end()) return false;
    }

    // 3. A loop holding both a and b must do so along ab.
    for (EdgeHandle outgoing : getOutgoingEdges(a)) {
        const std::vector<EdgeHandle> loop = getLoopEdges(outgoing);

        if (std::find(loop.begin(), loop.end(), ab) != loop.end()) continue;
        if (std::find(loop.begin(), loop.end(), ba) != loop.end()) continue;

        for (EdgeHandle edge : loop) {
            if (m_edges.get(edge).tip == b) return false;
        }
    }

    // 4. Don't pinch two borders together through the interior.
    const bool borderEdge = isBorder(ab) || isBorder(ba);
    if (!borderEdge && isBorderVertex(a) && isBorderVertex(b)) return false;

    return true;
}

void MeshData::collapseEdge(EdgeHandle ab, Vec3 position) {
    const EdgeHandle ba = m_edges.get(ab).pair;
    const VertexHandle a = getEdgeOrigin(ab);
    const VertexHandle b = getEdgeTip(ab);

    // 1. Remove ab and ba from their loops, collapsing triangles entirely.
    std::vector<VertexHandle> apexes;

    collapseSide(ab, apexes);
    collapseSide(ba, apexes);

    deleteEdgePair(ab);

    // 2. Everything that pointed at b points at a, and b is gone.
    retargetIncoming(b, a);
    m_vertices.remove(b);

    // 3. Fix outgoing edges that may have been deleted.
    repairVertexEdge(a);

    for (VertexHandle apex : apexes) {
        repairVertexEdge(apex);
    }

    m_vertices.get(a).position = position;
    setFacesDirtyByVertex(a);
}

void MeshData::collapseSide(EdgeHandle side, std::vector<VertexHandle>& apexes) {
    const std::vector<EdgeHandle> loop = getLoopEdges(side);
    const EdgeHandle pair = m_edges.get(side).pair;
    const bool wire = std::find(loop.begin(), loop.end(), pair) != loop.end();

    if (loop.size() != 3 || wire) {
        spliceOut(side);
        return;
    }

    // A triangle collapses to one edge: its two outer half-edges become a pair.
    const Edge& sideEdge = m_edges.get(side);
    const EdgeHandle next = sideEdge.next;
    const EdgeHandle prev = sideEdge.prev;
    const FaceHandle face = sideEdge.face;

    const EdgeHandle outerNext = m_edges.get(next).pair;
    const EdgeHandle outerPrev = m_edges.get(prev).pair;

    apexes.push_back(m_edges.get(next).tip);

    m_edges.get(outerNext).pair = outerPrev;
    m_edges.get(outerPrev).pair = outerNext;

    m_edges.remove(next);
    m_edges.remove(prev);

    if (isValidHandle(face)) m_faces.remove(face);
}
