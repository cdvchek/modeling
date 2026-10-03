#include "scene/mesh/mesh_data.hpp"

#include <algorithm>

bool MeshData::mergeVertices(VertexHandle a, VertexHandle b, u8 mergeType) {
    if (!isValidHandle(a) || !isValidHandle(b) || a == b) return false;

    // Only vertices joined by an edge can be merged.
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

        // a -> b -> a -> ... is a wire edge, not a triangle.
        if (std::find(loop.begin(), loop.end(), m_edges.get(side).pair) != loop.end()) continue;

        apexes.push_back(getEdgeTip(m_edges.get(side).next));
    }

    // Two triangles sharing an apex would fold into a single edge.
    if (apexes.size() == 2 && apexes[0] == apexes[1]) return false;

    // 2. Every vertex connected to both a and b must be a triangle apex.
    //    Otherwise the merged vertex would end up with two edges to it.
    const std::vector<VertexHandle> aNeighbors = getVertexNeighbors(a);
    const std::vector<VertexHandle> bNeighbors = getVertexNeighbors(b);

    for (VertexHandle neighbor : aNeighbors) {
        if (neighbor == b) continue;
        if (std::find(bNeighbors.begin(), bNeighbors.end(), neighbor) == bNeighbors.end()) continue;
        if (std::find(apexes.begin(), apexes.end(), neighbor) == apexes.end()) return false;
    }

    // 3. A face or border loop that holds both a and b, but not along ab,
    //    would visit the merged vertex twice.
    for (EdgeHandle outgoing : getOutgoingEdges(a)) {
        const std::vector<EdgeHandle> loop = getLoopEdges(outgoing);

        if (std::find(loop.begin(), loop.end(), ab) != loop.end()) continue;
        if (std::find(loop.begin(), loop.end(), ba) != loop.end()) continue;

        for (EdgeHandle edge : loop) {
            if (m_edges.get(edge).tip == b) return false;
        }
    }

    // 4. Two border vertices joined through the interior would pinch the
    //    surface into a single vertex where two borders touch.
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

    // A larger face (or border) just loses this side.
    if (loop.size() != 3 || wire) {
        spliceOut(side);
        return;
    }

    // A triangle x -> y -> c -> x collapses to a single edge between c and the
    // merged vertex. Its two other sides are deleted and their outer pairs
    // are paired with each other:
    //   outerNext (c -> y) and outerPrev (x -> c) become one edge.
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
