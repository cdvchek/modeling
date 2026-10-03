#include "scene/mesh/mesh_data.hpp"

#include <iostream>

namespace {
    bool fail(const char* kind, u32 index, const char* message) {
        std::cerr << "[mesh validate] " << kind << " " << index << ": " << message << std::endl;
        return false;
    }
}

bool MeshData::validate() const {
    const u32 edgeCount = m_edges.activeSize();

    // How many half-edges start at each vertex and belong to each face.
    // Compared against the fan and loop walks below to catch split fans
    // and faces with more than one loop.
    std::vector<u32> outgoingCounts(m_vertices.size(), 0);
    std::vector<u32> faceEdgeCounts(m_faces.size(), 0);

    // 1. Edges
    for (EdgeHandle handle : m_edges.getActiveHandles()) {
        const Edge& edge = m_edges.get(handle);
        const u32 i = handle.index;

        const Edge* pair = m_edges.tryGet(edge.pair);
        if (!pair) return fail("edge", i, "has no pair");
        if (edge.pair == handle) return fail("edge", i, "is paired with itself");
        if (pair->pair != handle) return fail("edge", i, "pair does not point back");

        if (!m_vertices.isValid(edge.tip)) return fail("edge", i, "tip is not a valid vertex");
        if (edge.tip == pair->tip) return fail("edge", i, "starts and ends at the same vertex");

        const Edge* next = m_edges.tryGet(edge.next);
        const Edge* prev = m_edges.tryGet(edge.prev);
        if (!next || !prev) return fail("edge", i, "next or prev is invalid");
        if (next->prev != handle) return fail("edge", i, "next.prev does not point back");
        if (prev->next != handle) return fail("edge", i, "prev.next does not point back");
        if (getEdgeOrigin(edge.next) != edge.tip) return fail("edge", i, "next does not start at this edge's tip");
        if (next->face != edge.face) return fail("edge", i, "next is on a different face");

        if (!edge.face.isNull()) {
            if (!m_faces.isValid(edge.face)) return fail("edge", i, "points at a deleted face");
            ++faceEdgeCounts[edge.face.index];
        }

        ++outgoingCounts[pair->tip.index];
    }

    // 2. Vertices
    for (VertexHandle handle : m_vertices.getActiveHandles()) {
        const Vertex& vertex = m_vertices.get(handle);
        const u32 i = handle.index;

        if (vertex.edge.isNull()) {
            if (outgoingCounts[i] != 0) return fail("vertex", i, "has edges but no outgoing edge set");
            continue;
        }

        if (!m_edges.isValid(vertex.edge)) return fail("vertex", i, "points at a deleted edge");
        if (getEdgeOrigin(vertex.edge) != handle) return fail("vertex", i, "edge does not start at this vertex");

        u32 steps = 0;
        EdgeHandle current = vertex.edge;

        do {
            if (getEdgeOrigin(current) != handle) return fail("vertex", i, "fan contains an edge that starts elsewhere");

            current = m_edges.get(m_edges.get(current).pair).next;

            if (++steps > edgeCount) return fail("vertex", i, "fan never returns to its start");
        } while (current != vertex.edge);

        if (steps != outgoingCounts[i]) return fail("vertex", i, "fan does not reach all of its outgoing edges");
    }

    // 3. Faces
    for (FaceHandle handle : m_faces.getActiveHandles()) {
        const Face& face = m_faces.get(handle);
        const u32 i = handle.index;

        const Edge* start = m_edges.tryGet(face.edge);
        if (!start) return fail("face", i, "points at a deleted edge");
        if (start->face != handle) return fail("face", i, "edge belongs to a different face");

        const std::vector<VertexHandle> corners = getFaceVertices(handle);
        const u32 sides = static_cast<u32>(corners.size());

        if (sides < 3) return fail("face", i, "has fewer than 3 sides");
        if (sides != faceEdgeCounts[i]) return fail("face", i, "has edges outside its loop");

        for (u32 c = 0; c < sides; ++c) {
            for (u32 d = c + 1; d < sides; ++d) {
                if (corners[c] == corners[d]) return fail("face", i, "visits the same vertex twice");
            }
        }
    }

    return true;
}
