#include "scene/mesh/mesh_data.hpp"

namespace {
    // Each half-edge is stored as five u32s: tip, pair, next, prev, face
    constexpr std::size_t EDGE_FIELDS = 5;

    // Old slot index to packed index, or INVALID_INDEX for slots that aren't in use
    template <typename T, typename H>
    std::vector<u32> packedIndices(const DynamicArray<T, H>& array, const std::vector<H>& handles) {
        std::vector<u32> packed(array.size(), INVALID_INDEX);
        for (u32 i = 0; i < handles.size(); ++i) packed[handles[i].index] = i;
        return packed;
    }

    template <typename T, typename H>
    u32 remap(const DynamicArray<T, H>& array, const std::vector<u32>& packed, H handle) {
        return array.isValid(handle) ? packed[handle.index] : INVALID_INDEX;
    }

    // A stored link is a packed index below count, or INVALID_INDEX when optional
    bool linkOk(u32 index, u32 count, bool optional) {
        return index < count || (optional && index == INVALID_INDEX);
    }

    template <typename H>
    H toHandle(u32 index) {
        return index == INVALID_INDEX ? H { INVALID_INDEX, 0 } : H { index, 0 };
    }
}

void MeshData::writeTo(BinaryWriter& writer) const {
    const std::vector<VertexHandle> vertices = m_vertices.getActiveHandles();
    const std::vector<EdgeHandle> edges = m_edges.getActiveHandles();
    const std::vector<FaceHandle> faces = m_faces.getActiveHandles();

    const std::vector<u32> vertexIndex = packedIndices(m_vertices, vertices);
    const std::vector<u32> edgeIndex = packedIndices(m_edges, edges);
    const std::vector<u32> faceIndex = packedIndices(m_faces, faces);

    std::vector<f32> positions;
    std::vector<u32> vertexEdges;
    positions.reserve(vertices.size() * 3);
    vertexEdges.reserve(vertices.size());

    for (VertexHandle handle : vertices) {
        const Vertex& vertex = m_vertices.get(handle);
        positions.insert(positions.end(), { vertex.position.x, vertex.position.y, vertex.position.z });
        vertexEdges.push_back(remap(m_edges, edgeIndex, vertex.edge));
    }

    std::vector<u32> edgeLinks;
    edgeLinks.reserve(edges.size() * EDGE_FIELDS);

    for (EdgeHandle handle : edges) {
        const Edge& edge = m_edges.get(handle);
        edgeLinks.insert(edgeLinks.end(), {
            remap(m_vertices, vertexIndex, edge.tip),
            remap(m_edges, edgeIndex, edge.pair),
            remap(m_edges, edgeIndex, edge.next),
            remap(m_edges, edgeIndex, edge.prev),
            remap(m_faces, faceIndex, edge.face),
        });
    }

    std::vector<u32> faceEdges;
    faceEdges.reserve(faces.size());
    for (FaceHandle handle : faces) faceEdges.push_back(remap(m_edges, edgeIndex, m_faces.get(handle).edge));

    writer.reserve(writer.size() + 12 + positions.size() * 4 + (vertexEdges.size() + edgeLinks.size() + faceEdges.size()) * 4);
    writer.write(static_cast<u32>(vertices.size()));
    writer.write(static_cast<u32>(edges.size()));
    writer.write(static_cast<u32>(faces.size()));
    writer.writeArray(positions.data(), positions.size());
    writer.writeArray(vertexEdges.data(), vertexEdges.size());
    writer.writeArray(edgeLinks.data(), edgeLinks.size());
    writer.writeArray(faceEdges.data(), faceEdges.size());
}

bool MeshData::readFrom(BinaryReader& reader) {
    u32 vertexCount = 0;
    u32 edgeCount = 0;
    u32 faceCount = 0;
    if (!reader.read(vertexCount) || !reader.read(edgeCount) || !reader.read(faceCount)) return false;

    std::vector<f32> positions;
    std::vector<u32> vertexEdges;
    std::vector<u32> edgeLinks;
    std::vector<u32> faceEdges;

    if (!reader.readVector(positions, std::size_t(vertexCount) * 3)) return false;
    if (!reader.readVector(vertexEdges, vertexCount)) return false;
    if (!reader.readVector(edgeLinks, std::size_t(edgeCount) * EDGE_FIELDS)) return false;
    if (!reader.readVector(faceEdges, faceCount)) return false;

    // Every link must point inside the mesh before any handle is built from it
    for (u32 edge : vertexEdges) if (!linkOk(edge, edgeCount, true)) return false;
    for (u32 edge : faceEdges) if (!linkOk(edge, edgeCount, false)) return false;
    for (std::size_t i = 0; i < edgeLinks.size(); i += EDGE_FIELDS) {
        if (!linkOk(edgeLinks[i], vertexCount, false)) return false;
        for (std::size_t k = 1; k < 4; ++k) if (!linkOk(edgeLinks[i + k], edgeCount, false)) return false;
        if (!linkOk(edgeLinks[i + 4], faceCount, true)) return false;
    }

    // Inserting into empty arrays gives slot i generation 0, matching the packed indices
    DynamicArray<Vertex, VertexHandle> vertices;
    DynamicArray<Edge, EdgeHandle> edges;
    DynamicArray<Face, FaceHandle> faces;
    vertices.reserve(vertexCount);
    edges.reserve(edgeCount);
    faces.reserve(faceCount);

    for (u32 i = 0; i < vertexCount; ++i) {
        Vertex vertex;
        vertex.position = Vec3(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
        vertex.edge = toHandle<EdgeHandle>(vertexEdges[i]);
        vertices.insert(vertex);
    }

    for (u32 i = 0; i < edgeCount; ++i) {
        const u32* links = &edgeLinks[std::size_t(i) * EDGE_FIELDS];
        Edge edge;
        edge.tip = toHandle<VertexHandle>(links[0]);
        edge.pair = toHandle<EdgeHandle>(links[1]);
        edge.next = toHandle<EdgeHandle>(links[2]);
        edge.prev = toHandle<EdgeHandle>(links[3]);
        edge.face = toHandle<FaceHandle>(links[4]);
        edges.insert(edge);
    }

    for (u32 i = 0; i < faceCount; ++i) {
        Face face;
        face.edge = toHandle<EdgeHandle>(faceEdges[i]);
        faces.insert(face);
    }

    m_vertices = std::move(vertices);
    m_edges = std::move(edges);
    m_faces = std::move(faces);
    m_dirty = true;
    return true;
}
