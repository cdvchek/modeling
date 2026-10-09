#include "scene/mesh/mesh_data.hpp"

#include <cmath>
#include "scene/mesh/mesh_factory.hpp"

void MeshData::setMesh(PresetMesh meshType) {
    PackagedMesh pMesh;

    switch(meshType) {
        case PresetMesh::Cube:      pMesh = MeshFactory::cube(); break;
        case PresetMesh::Plane:     pMesh = MeshFactory::plane(); break;
        case PresetMesh::Grid:      pMesh = MeshFactory::grid(); break;
        case PresetMesh::Circle:    pMesh = MeshFactory::circle(); break;
        case PresetMesh::Cylinder:  pMesh = MeshFactory::cylinder(); break;
        case PresetMesh::Cone:      pMesh = MeshFactory::cone(); break;
        case PresetMesh::UVSphere:  pMesh = MeshFactory::uvSphere(); break;
        case PresetMesh::IcoSphere: pMesh = MeshFactory::icoSphere(); break;
        case PresetMesh::Torus:     pMesh = MeshFactory::torus(); break;
    }

    m_vertices = pMesh.vertices;
    m_edges = pMesh.edges;
    m_faces = pMesh.faces;

    m_dirty = true;
}

void MeshData::setMesh(const PackagedMesh& mesh) {
    m_vertices = mesh.vertices;
    m_edges = mesh.edges;
    m_faces = mesh.faces;

    m_dirty = true;
}

const Vertex* MeshData::getVertex(VertexHandle handle) const {
    if (!m_vertices.isValid(handle))
        return nullptr;

    return &m_vertices.get(handle);
}

const Edge* MeshData::getEdge(EdgeHandle handle) const {
    if (!m_edges.isValid(handle))
        return nullptr;

    return &m_edges.get(handle);
}

const Face* MeshData::getFace(FaceHandle handle) const {
    if (!m_faces.isValid(handle))
        return nullptr;

    return &m_faces.get(handle);
}

const std::vector<Vertex> MeshData::getVertices() const {
    return m_vertices.getActiveValues();
}

const std::vector<Face> MeshData::getFaces() const {
    return m_faces.getActiveValues();
}

const std::vector<VertexHandle> MeshData::getVertexHandles() const {
    return m_vertices.getActiveHandles();
}

const std::vector<EdgeHandle> MeshData::getEdgeHandles() const {
    return m_edges.getActiveHandles();
}

const std::vector<FaceHandle> MeshData::getFaceHandles() const {
    return m_faces.getActiveHandles();
}

VertexHandle MeshData::getEdgeOrigin(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    if (!edge) return INVALID_VERTEX;

    const Edge* pair = m_edges.tryGet(edge->pair);
    if (!pair || !m_vertices.isValid(pair->tip)) return INVALID_VERTEX;

    return pair->tip;
}

VertexHandle MeshData::getEdgeTip(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    if (!edge) return INVALID_VERTEX;

    if (m_vertices.isValid(edge->tip)) return edge->tip;
    else return INVALID_VERTEX;
}

bool MeshData::isValidHandle(VertexHandle handle) const {
    return m_vertices.isValid(handle);
}

bool MeshData::isValidHandle(EdgeHandle handle) const {
    return m_edges.isValid(handle);
}

bool MeshData::isValidHandle(FaceHandle handle) const {
    return m_faces.isValid(handle);
}

MaterialHandle MeshData::getFaceMaterial(FaceHandle handle) const {
    const Face* face = m_faces.tryGet(handle);
    return face ? face->material : INVALID_MATERIAL;
}

void MeshData::setFaceMaterial(FaceHandle handle, MaterialHandle material) {
    Face* face = m_faces.tryGet(handle);
    if (!face) return;
    face->material = material;
    // Faces regroup by material on the GPU, so the layout changes
    m_materialStamp = nextStructureStamp();
}

MaterialHandle MeshData::sharedFaceMaterial(const std::vector<FaceHandle>& faces) const {
    MaterialHandle shared = INVALID_MATERIAL;
    for (std::size_t i = 0; i < faces.size(); ++i) {
        const MaterialHandle material = getFaceMaterial(faces[i]);
        if (i == 0) shared = material;
        else if (!(material == shared)) return INVALID_MATERIAL;
    }
    return shared;
}

std::vector<Vec2> MeshData::getFaceUVs(FaceHandle handle) const {
    std::vector<Vec2> uvs;
    for (EdgeHandle edge : getFaceEdges(handle)) uvs.push_back(m_edges.get(edge).uv);
    return uvs;
}

void MeshData::setFaceUVs(FaceHandle handle, const std::vector<Vec2>& uvs) {
    const std::vector<EdgeHandle> edges = getFaceEdges(handle);
    if (edges.size() != uvs.size()) return;
    for (std::size_t i = 0; i < edges.size(); ++i) m_edges.get(edges[i]).uv = uvs[i];
    // The GPU copy's corners change everywhere on the face, so it's rebuilt
    m_uvStamp = nextStructureStamp();
}

std::vector<FaceHandle> MeshData::getUVIsland(FaceHandle handle) const {
    std::vector<FaceHandle> island;
    if (!m_faces.isValid(handle)) return island;

    // UVs this close count as the same corner (a seam has them apart)
    constexpr f32 SAME_UV = 1e-5f;
    const auto same = [](const Vec2& a, const Vec2& b) { return std::fabs(a.x - b.x) <= SAME_UV && std::fabs(a.y - b.y) <= SAME_UV; };

    std::vector<bool> seen(m_faces.size(), false);
    std::vector<FaceHandle> pending { handle };
    seen[handle.index] = true;
    while (!pending.empty()) {
        const FaceHandle face = pending.back();
        pending.pop_back();
        island.push_back(face);

        for (EdgeHandle edge : getFaceEdges(face)) {
            // This side: origin's UV on the previous half-edge, tip's on this one; the other side runs the other way
            const Edge& half = m_edges.get(edge);
            const Edge* pair = m_edges.tryGet(half.pair);
            if (!pair || !m_faces.isValid(pair->face) || seen[pair->face.index]) continue;
            const Edge* pairPrev = m_edges.tryGet(pair->prev);
            const Edge* prev = m_edges.tryGet(half.prev);
            if (!pairPrev || !prev) continue;

            if (same(prev->uv, pair->uv) && same(half.uv, pairPrev->uv)) {
                seen[pair->face.index] = true;
                pending.push_back(pair->face);
            }
        }
    }
    return island;
}

bool MeshData::isSeam(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    return edge && edge->seam;
}

void MeshData::setSeam(EdgeHandle handle, bool seam) {
    Edge* edge = m_edges.tryGet(handle);
    if (!edge) return;
    edge->seam = seam;
    if (Edge* pair = m_edges.tryGet(edge->pair)) pair->seam = seam;
}

std::vector<EdgeHandle> MeshData::getSeamEdges() const {
    std::vector<EdgeHandle> seams;
    for (EdgeHandle handle : m_edges.getActiveHandles()) {
        const Edge& edge = m_edges.get(handle);
        if (edge.seam && !(edge.pair.index < handle.index && m_edges.isValid(edge.pair))) seams.push_back(handle);
    }
    return seams;
}

std::vector<bool> MeshData::getFaceEdgeSeams(FaceHandle handle) const {
    std::vector<bool> seams;
    for (EdgeHandle edge : getFaceEdges(handle)) seams.push_back(m_edges.get(edge).seam);
    return seams;
}

std::vector<bool> MeshData::getEdgeSeams() const {
    std::vector<bool> seams;
    for (EdgeHandle handle : m_edges.getActiveHandles()) seams.push_back(m_edges.get(handle).seam);
    return seams;
}

void MeshData::setEdgeSeams(const std::vector<bool>& seams) {
    const std::vector<EdgeHandle> edges = m_edges.getActiveHandles();
    if (edges.size() != seams.size()) return;
    for (std::size_t i = 0; i < edges.size(); ++i) m_edges.get(edges[i]).seam = seams[i];
}

bool MeshData::hasSeams() const {
    for (EdgeHandle handle : m_edges.getActiveHandles()) if (m_edges.get(handle).seam) return true;
    return false;
}

u32 MeshData::markSeamsFromIslands() {
    constexpr f32 SAME_UV = 1e-5f;
    const auto same = [](const Vec2& a, const Vec2& b) { return std::fabs(a.x - b.x) <= SAME_UV && std::fabs(a.y - b.y) <= SAME_UV; };

    u32 marked = 0;
    for (EdgeHandle handle : m_edges.getActiveHandles()) {
        Edge& half = m_edges.get(handle);
        const Edge* pair = m_edges.tryGet(half.pair);
        // Each edge once, between two faces, not already a seam
        if (half.seam || !pair || half.pair.index < handle.index || !m_faces.isValid(half.face) || !m_faces.isValid(pair->face)) continue;
        const Edge* prev = m_edges.tryGet(half.prev);
        const Edge* pairPrev = m_edges.tryGet(pair->prev);
        if (!prev || !pairPrev) continue;

        // The same two corners, as each side's face has them
        if (!same(prev->uv, pair->uv) || !same(half.uv, pairPrev->uv)) {
            setSeam(handle, true);
            ++marked;
        }
    }
    return marked;
}

std::vector<Vec2> MeshData::getCornerUVs() const {
    std::vector<Vec2> uvs;
    for (EdgeHandle edge : m_edges.getActiveHandles()) uvs.push_back(m_edges.get(edge).uv);
    return uvs;
}

void MeshData::setCornerUVs(const std::vector<Vec2>& uvs) {
    const std::vector<EdgeHandle> edges = m_edges.getActiveHandles();
    if (edges.size() != uvs.size()) return;
    for (std::size_t i = 0; i < edges.size(); ++i) m_edges.get(edges[i]).uv = uvs[i];
    m_uvStamp = nextStructureStamp();
}
