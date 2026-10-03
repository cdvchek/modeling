#include "scene/mesh/mesh_data.hpp"

FaceHandle MeshData::insertFaceRing(FaceHandle handle) {
    // The original face keeps its handle and becomes the top face.
    // Its half-edges stay where they are and become the bottom edges
    // of the new side quads, so the neighboring faces are untouched.

    // 1. Get the face's loop. bottom[i] runs oldVerts[i] -> oldVerts[i + 1].
    const std::vector<EdgeHandle> bottom = getFaceEdges(handle);
    const u32 count = static_cast<u32>(bottom.size());

    if (count < 3) return INVALID_FACE;

    std::vector<VertexHandle> oldVerts(count);

    for (u32 i = 0; i < count; ++i) {
        oldVerts[i] = getEdgeOrigin(bottom[i]);
        if (!isValidHandle(oldVerts[i])) return INVALID_FACE;
    }

    // 2. Duplicate the vertices.
    std::vector<VertexHandle> newVerts(count);

    for (u32 i = 0; i < count; ++i) {
        newVerts[i] = addVertex(m_vertices.get(oldVerts[i]).position);
    }

    // 3. Create the vertical edges (up[i]: oldVerts[i] -> newVerts[i])
    //    and the top edges (top[i]: newVerts[i] -> newVerts[i + 1]).
    std::vector<EdgeHandle> up(count);
    std::vector<EdgeHandle> top(count);

    for (u32 i = 0; i < count; ++i) {
        u32 next = (i + 1) % count;

        up[i] = addEdgePair(oldVerts[i], newVerts[i]);
        top[i] = addEdgePair(newVerts[i], newVerts[next]);
    }

    // 4. Build each side quad:
    //    oldVerts[i] -> oldVerts[i + 1] -> newVerts[i + 1] -> newVerts[i] -> oldVerts[i]
    for (u32 i = 0; i < count; ++i) {
        u32 next = (i + 1) % count;

        const EdgeHandle down = m_edges.get(up[i]).pair;
        const EdgeHandle topTwin = m_edges.get(top[i]).pair;

        link(bottom[i], up[next]);
        link(up[next], topTwin);
        link(topTwin, down);
        link(down, bottom[i]);

        const FaceHandle side = m_faces.insert(Face{});
        assignFace(bottom[i], side);
    }

    // 5. Link the top loop and give it the original face.
    for (u32 i = 0; i < count; ++i) {
        link(top[i], top[(i + 1) % count]);
    }

    assignFace(top[0], handle);

    // 6. Give each new vertex an outgoing edge.
    for (u32 i = 0; i < count; ++i) {
        m_vertices.get(newVerts[i]).edge = top[i];
    }

    return handle;
}

VertexHandle MeshData::splitEdge(EdgeHandle handle) {
    if (!isValidHandle(handle)) return INVALID_VERTEX;

    const EdgeHandle pairHandle = m_edges.get(handle).pair;
    if (!isValidHandle(pairHandle)) return INVALID_VERTEX;

    const VertexHandle origin = getEdgeOrigin(handle);
    const VertexHandle tip = m_edges.get(handle).tip;
    if (!isValidHandle(origin) || !isValidHandle(tip)) return INVALID_VERTEX;

    const EdgeHandle edgeNext = m_edges.get(handle).next;
    const EdgeHandle pairPrev = m_edges.get(pairHandle).prev;

    // 1. Create the midpoint and the second half of the edge (mid -> tip, tip -> mid).
    const VertexHandle mid = addVertex((getVertexPosition(origin) + getVertexPosition(tip)) / 2);
    const EdgeHandle second = addEdgePair(mid, tip);
    const EdgeHandle secondPair = m_edges.get(second).pair;

    m_edges.get(second).face = m_edges.get(handle).face;
    m_edges.get(secondPair).face = m_edges.get(pairHandle).face;

    // 2. The original pair becomes origin -> mid / mid -> origin.
    m_edges.get(handle).tip = mid;

    // 3. Splice the new half-edges in. If tip had no other edges,
    //    the loop turns around at tip: second -> secondPair.
    const EdgeHandle after = edgeNext == pairHandle ? secondPair : edgeNext;
    const EdgeHandle before = pairPrev == handle ? second : pairPrev;

    link(handle, second);
    link(second, after);
    link(before, secondPair);
    link(secondPair, pairHandle);

    // 4. pairHandle now starts at mid, so tip may need a new outgoing edge.
    m_vertices.get(mid).edge = pairHandle;
    if (m_vertices.get(tip).edge == pairHandle) m_vertices.get(tip).edge = secondPair;

    setFacesDirtyByVertex(mid);

    return mid;
}

bool MeshData::removeVertex(VertexHandle handle) {
    if (!isValidHandle(handle)) return false;

    // removeEdge also removes the faces on both sides of each edge.
    for (EdgeHandle edge : getOutgoingEdges(handle)) {
        if (!removeEdge(edge)) return false;
    }

    m_vertices.remove(handle);

    return true;
}

bool MeshData::removeEdge(EdgeHandle handle) {
    if (!isValidHandle(handle)) return false;

    // Copy both half-edges; their storage is removed below.
    const Edge edge = m_edges.get(handle);
    if (!isValidHandle(edge.pair)) return false;

    const EdgeHandle pairHandle = edge.pair;
    const Edge pair = m_edges.get(pairHandle);

    const VertexHandle origin = pair.tip;
    const VertexHandle tip = edge.tip;

    // 1. Remove adjacent faces while their loops are still intact.
    if (isValidHandle(edge.face)) removeFace(edge.face);
    if (isValidHandle(pair.face)) removeFace(pair.face);

    // 2. Splice both half-edges out, joining the two loops into one.
    //    If this is the last edge at a vertex, that side has nothing to join.
    if (edge.next != pairHandle) link(pair.prev, edge.next);
    if (pair.next != handle) link(edge.prev, pair.next);

    // 3. Delete the half-edges and give both endpoints a surviving outgoing edge.
    deleteEdgePair(handle);

    repairVertexEdge(origin);
    repairVertexEdge(tip);

    return true;
}

bool MeshData::removeFace(FaceHandle handle) {
    if (!isValidHandle(handle)) return false;
    const Face& face = m_faces.get(handle);

    if (!isValidHandle(face.edge)) return false;

    // The loop stays behind as a border.
    assignFace(face.edge, INVALID_FACE);
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
    assignFace(start, newFace);

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
