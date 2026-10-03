#include "scene/mesh/mesh_data.hpp"
#include "core/math/math_utils.hpp"

#include <cmath>
#include <algorithm>

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
