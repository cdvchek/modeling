#include "scene/mesh/mesh_data.hpp"

FaceHandle MeshData::insertFaceRing(FaceHandle handle) {

    // 1. Get the face's loop. bottom[i] runs oldVerts[i] -> oldVerts[i + 1].
    const std::vector<EdgeHandle> bottom = getFaceEdges(handle);
    const u32 count = static_cast<u32>(bottom.size());

    if (count < 3) return INVALID_FACE;

    // The face's UV at each old vertex: bottom[i] ends at oldVerts[i + 1], so it holds that corner's
    std::vector<Vec2> cornerUV(count);
    for (u32 i = 0; i < count; ++i) cornerUV[(i + 1) % count] = m_edges.get(bottom[i]).uv;

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

    // 3. Create the vertical edges (up) and the top edges (top).
    std::vector<EdgeHandle> up(count);
    std::vector<EdgeHandle> top(count);

    for (u32 i = 0; i < count; ++i) {
        u32 next = (i + 1) % count;

        up[i] = addEdgePair(oldVerts[i], newVerts[i]);
        top[i] = addEdgePair(newVerts[i], newVerts[next]);
    }

    // 4. Build each side quad.
    for (u32 i = 0; i < count; ++i) {
        u32 next = (i + 1) % count;

        const EdgeHandle down = m_edges.get(up[i]).pair;
        const EdgeHandle topTwin = m_edges.get(top[i]).pair;

        link(bottom[i], up[next]);
        link(up[next], topTwin);
        link(topTwin, down);
        link(down, bottom[i]);

        // A side copies the UVs of the edge it grew from, both rows the same (a strip with no height to unwrap later)
        m_edges.get(up[next]).uv = cornerUV[next];
        m_edges.get(topTwin).uv = cornerUV[i];
        m_edges.get(down).uv = cornerUV[i];

        Face sideFace;
        sideFace.material = m_faces.get(handle).material;
        const FaceHandle side = m_faces.insert(sideFace);
        assignFace(bottom[i], side);
    }

    // 5. Link the top loop and give it the original face, with its UVs.
    for (u32 i = 0; i < count; ++i) {
        link(top[i], top[(i + 1) % count]);
        m_edges.get(top[i]).uv = cornerUV[(i + 1) % count];
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

    // UVs on each side: the corners at either end of the edge, in that side's face
    const Vec2 tipUV = m_edges.get(handle).uv;
    const Vec2 originUV = isValidHandle(m_edges.get(handle).prev) ? m_edges.get(m_edges.get(handle).prev).uv : tipUV;
    const Vec2 pairOriginUV = m_edges.get(pairHandle).uv;
    const Vec2 pairTipUV = isValidHandle(pairPrev) ? m_edges.get(pairPrev).uv : pairOriginUV;

    // 1. Create the midpoint and the second half of the edge (mid -> tip, tip -> mid).
    const VertexHandle mid = addVertex((getVertexPosition(origin) + getVertexPosition(tip)) / 2);
    const EdgeHandle second = addEdgePair(mid, tip);
    const EdgeHandle secondPair = m_edges.get(second).pair;

    m_edges.get(second).face = m_edges.get(handle).face;
    m_edges.get(secondPair).face = m_edges.get(pairHandle).face;

    // The new corner sits halfway along the edge on each side
    m_edges.get(handle).uv = (originUV + tipUV) * 0.5f;
    m_edges.get(second).uv = tipUV;
    m_edges.get(secondPair).uv = (pairTipUV + pairOriginUV) * 0.5f;
    // Both pieces keep the edge's mark
    m_edges.get(second).mark = m_edges.get(secondPair).mark = m_edges.get(handle).mark;
    m_edges.get(second).seam = m_edges.get(secondPair).seam = m_edges.get(handle).seam;

    // 2. The original pair becomes origin -> mid / mid -> origin.
    m_edges.get(handle).tip = mid;

    // 3. Splice the new half-edges in.
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

    for (EdgeHandle edge : getOutgoingEdges(handle)) {
        if (!removeEdge(edge)) return false;
    }

    m_vertices.remove(handle);

    return true;
}

bool MeshData::removeEdge(EdgeHandle handle) {
    if (!isValidHandle(handle)) return false;

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

    assignFace(face.edge, INVALID_FACE);
    m_faces.remove(handle);

    return true;
}

bool MeshData::fillFaceLoop(EdgeHandle handle, const MaterialHandle* material, const std::unordered_map<u32, Vec2>* uvByVertex) {
    if (!isValidHandle(handle)) return false;
    Edge& edge = m_edges.get(handle);

    if (isValidHandle(edge.face)) {
        if (!isValidHandle(edge.pair)) return false;

        handle = edge.pair;

        if (!isValidHandle(handle)) return false;
        Edge& pair = m_edges.get(handle);

        if (isValidHandle(pair.face)) return false;
    }

    const EdgeHandle start = handle;
    EdgeHandle current = start;

    // Pass 1: validate the border loop
    do {
        if (!isValidHandle(current)) return false;
        const Edge& currentEdge = m_edges.get(current);

        if (isValidHandle(currentEdge.face)) return false;
        
        if (!isValidHandle(currentEdge.next)) return false;
        current = currentEdge.next;
    } while (current != start);

    // The faces across the loop decide the new face's material, unless the caller already knows it
    Face filled { start };
    if (material) {
        filled.material = *material;
    } else {
        std::vector<FaceHandle> neighbors;
        current = start;
        do {
            const FaceHandle across = m_edges.get(m_edges.get(current).pair).face;
            if (isValidHandle(across)) neighbors.push_back(across);
            current = m_edges.get(current).next;
        } while (current != start);
        filled.material = sharedFaceMaterial(neighbors);
    }

    FaceHandle newFace = m_faces.insert(filled);
    assignFace(start, newFace);

    // Each corner: the given UV for its vertex, or the one the face across the loop has there (in that face, the
    // half-edge ending at this corner's vertex comes just before the pair)
    current = start;
    do {
        Edge& edge = m_edges.get(current);
        bool given = false;
        if (uvByVertex) {
            const auto found = uvByVertex->find(edge.tip.index);
            if (found != uvByVertex->end()) {
                edge.uv = found->second;
                given = true;
            }
        }
        if (!given) {
            const Edge& pair = m_edges.get(edge.pair);
            if (isValidHandle(pair.face) && isValidHandle(pair.prev)) edge.uv = m_edges.get(pair.prev).uv;
        }
        current = edge.next;
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

    if (splittingFace && bPrev.face != face) return false;

    // 6. Remove the old face before changing its loop; both halves keep its material and its corners' UVs.
    const MaterialHandle material = splittingFace ? m_faces.get(face).material : INVALID_MATERIAL;
    std::unordered_map<u32, Vec2> uvByVertex;
    if (splittingFace) {
        for (EdgeHandle edge : getFaceEdges(face)) uvByVertex[m_edges.get(edge).tip.index] = m_edges.get(edge).uv;
    }
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
        if (!fillFaceLoop(newAH, &material, &uvByVertex)) return false;
        if (!fillFaceLoop(newBH, &material, &uvByVertex)) return false;
    }

    return true;
}
