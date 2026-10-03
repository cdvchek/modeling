#include "scene/mesh/mesh_data.hpp"

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
