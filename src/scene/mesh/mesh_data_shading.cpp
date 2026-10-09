#include "scene/mesh/mesh_data.hpp"
#include "core/math/math_utils.hpp"

#include <algorithm>
#include <cmath>

namespace {
    // How far a face's corners may sit off its plane, relative to its size, and still count as flat
    constexpr f32 PLANAR_TOLERANCE = 1e-5f;
    // A vertex has at most this many faces in one fan walk; guards against a damaged loop
    constexpr u32 MAX_FAN = 4096;
}

void MeshData::setShading(ShadingMode mode) {
    if (mode == m_shading) return;
    m_shading = mode;
    m_shadingStamp = nextStructureStamp();
}

void MeshData::setSmoothAngle(f32 radians) {
    radians = std::clamp(radians, 0.0f, Math::PI);
    if (radians == m_smoothAngle) return;
    m_smoothAngle = radians;
    m_shadingStamp = nextStructureStamp();
}

EdgeMark MeshData::getEdgeMark(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    return edge ? edge->mark : EdgeMark::None;
}

void MeshData::setEdgeMark(EdgeHandle handle, EdgeMark mark) {
    Edge* edge = m_edges.tryGet(handle);
    if (!edge) return;
    edge->mark = mark;
    if (Edge* pair = m_edges.tryGet(edge->pair)) pair->mark = mark;
    m_shadingStamp = nextStructureStamp();
}

std::vector<EdgeMark> MeshData::getFaceEdgeMarks(FaceHandle handle) const {
    std::vector<EdgeMark> marks;
    for (EdgeHandle edge : getFaceEdges(handle)) marks.push_back(m_edges.get(edge).mark);
    return marks;
}

std::vector<EdgeMark> MeshData::getEdgeMarks() const {
    std::vector<EdgeMark> marks;
    for (EdgeHandle edge : m_edges.getActiveHandles()) marks.push_back(m_edges.get(edge).mark);
    return marks;
}

void MeshData::setEdgeMarks(const std::vector<EdgeMark>& marks) {
    const std::vector<EdgeHandle> edges = m_edges.getActiveHandles();
    if (edges.size() != marks.size()) return;
    for (std::size_t i = 0; i < edges.size(); ++i) m_edges.get(edges[i]).mark = marks[i];
    m_shadingStamp = nextStructureStamp();
}

bool MeshData::hasEdgeMarks() const {
    for (EdgeHandle edge : m_edges.getActiveHandles()) if (m_edges.get(edge).mark != EdgeMark::None) return true;
    return false;
}

bool MeshData::isEdgeHard(EdgeHandle handle) const {
    const Edge* edge = m_edges.tryGet(handle);
    if (!edge) return true;
    const Edge* pair = m_edges.tryGet(edge->pair);
    if (!pair || !m_faces.isValid(edge->face) || !m_faces.isValid(pair->face)) return true;

    if (m_shading == ShadingMode::Flat) return true;
    if (edge->mark == EdgeMark::Hard) return true;
    if (edge->mark == EdgeMark::Smooth) return false;
    if (m_shading == ShadingMode::Smooth) return false;

    // Auto: the angle between the two faces' normals
    const Vec3 a = getFaceNormal(edge->face);
    const Vec3 b = getFaceNormal(pair->face);
    return Vec3::dot(a, b) < std::cos(m_smoothAngle) - 1e-6f;
}

std::vector<EdgeHandle> MeshData::getHardEdges() const {
    std::vector<EdgeHandle> hard;
    if (m_shading == ShadingMode::Flat) return hard;

    for (EdgeHandle handle : m_edges.getActiveHandles()) {
        const Edge& edge = m_edges.get(handle);
        // Each edge once (the lower half), and only between two faces: borders are hard anyway
        if (edge.pair.index < handle.index || !m_faces.isValid(edge.face)) continue;
        const Edge* pair = m_edges.tryGet(edge.pair);
        if (!pair || !m_faces.isValid(pair->face)) continue;
        if (isEdgeHard(handle)) hard.push_back(handle);
    }
    return hard;
}

Vec3 MeshData::getCornerNormal(EdgeHandle incoming) const {
    const Edge* first = m_edges.tryGet(incoming);
    if (!first) return Vec3(0.0f);
    if (m_shading == ShadingMode::Flat) return getFaceNormal(first->face);

    // Back across soft edges to the fan's first face, or round to the lowest handle when the fan closes,
    // so every corner of one fan sums the same faces in the same order and gets exactly the same normal
    EdgeHandle start = incoming;
    EdgeHandle lowest = incoming;
    bool closed = false;
    for (u32 step = 0; step < MAX_FAN; ++step) {
        if (isEdgeHard(start)) break;
        // A soft edge has a face on its other side, whose half-edge into the same vertex comes before the pair
        const EdgeHandle back = m_edges.get(m_edges.get(start).pair).prev;
        if (!m_edges.isValid(back)) break;
        if (back == incoming) { closed = true; break; }
        start = back;
        if (start.index < lowest.index) lowest = start;
    }
    if (closed) start = lowest;

    // Forward, adding each face's normal weighted by its corner angle
    Vec3 sum(0.0f);
    EdgeHandle current = start;
    for (u32 step = 0; step < MAX_FAN; ++step) {
        const Edge& edge = m_edges.get(current);
        const Vec3 corner = m_vertices.get(edge.tip).position;
        const Vec3 toPrevious = m_vertices.get(getEdgeOrigin(current)).position - corner;
        const Vec3 toNext = m_vertices.get(m_edges.get(edge.next).tip).position - corner;
        const f32 lengths = toPrevious.length() * toNext.length();
        const f32 angle = lengths > Math::EPSILON ? std::acos(std::clamp(Vec3::dot(toPrevious, toNext) / lengths, -1.0f, 1.0f)) : 0.0f;
        sum = sum + getFaceNormal(edge.face) * angle;

        const EdgeHandle out = edge.next;
        if (isEdgeHard(out)) break;
        current = m_edges.get(out).pair;
        if (current == start || !m_edges.isValid(current)) break;
    }

    const f32 length = sum.length();
    return length > Math::EPSILON ? sum / length : getFaceNormal(first->face);
}

bool MeshData::isFacePlanar(FaceHandle handle) const {
    const Vec3 normal = getFaceNormal(handle);
    if (Vec3::dot(normal, normal) < 0.5f) return false;

    const std::vector<VertexHandle> corners = getFaceVertices(handle);
    if (corners.empty()) return false;
    const Vec3 origin = getVertexPosition(corners[0]);

    f32 size = 0.0f;
    for (VertexHandle corner : corners) size = std::max(size, (getVertexPosition(corner) - origin).length());

    for (VertexHandle corner : corners) {
        if (std::fabs(Vec3::dot(getVertexPosition(corner) - origin, normal)) > PLANAR_TOLERANCE * size) return false;
    }
    return true;
}

std::vector<FaceHandle> MeshData::getFacesToPatch(const std::vector<VertexHandle>& moved) const {
    std::vector<FaceHandle> faces;
    for (VertexHandle vertex : moved) {
        const std::vector<FaceHandle> around = getVertexFaces(vertex);
        faces.insert(faces.end(), around.begin(), around.end());
    }

    // A moved vertex turns its faces, which changes the smooth normals at every corner of those faces
    if (m_shading != ShadingMode::Flat) {
        std::vector<VertexHandle> corners;
        for (FaceHandle face : faces) {
            const std::vector<VertexHandle> vertices = getFaceVertices(face);
            corners.insert(corners.end(), vertices.begin(), vertices.end());
        }
        std::sort(corners.begin(), corners.end(), [](VertexHandle a, VertexHandle b) { return a.index < b.index; });
        corners.erase(std::unique(corners.begin(), corners.end()), corners.end());
        for (VertexHandle vertex : corners) {
            const std::vector<FaceHandle> around = getVertexFaces(vertex);
            faces.insert(faces.end(), around.begin(), around.end());
        }
    }

    std::sort(faces.begin(), faces.end(), [](FaceHandle a, FaceHandle b) { return a.index < b.index; });
    faces.erase(std::unique(faces.begin(), faces.end()), faces.end());
    return faces;
}
