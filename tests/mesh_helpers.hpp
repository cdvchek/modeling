#pragma once

#include "scene/mesh/mesh_data.hpp"

#include <algorithm>
#include <string>

inline MeshData cube() {
    MeshData mesh;
    mesh.setMesh(PresetMesh::Cube);
    return mesh;
}

// Extrudes the face and pulls its new top outward so the sides have area.
inline void extrude(MeshData& mesh, FaceHandle face, f32 distance = 0.5f) {
    mesh.insertFaceRing(face);

    const Vec3 normal = mesh.getFaceNormal(face).normalized();
    for (VertexHandle vertex : mesh.getFaceVertices(face)) mesh.translateVertex(vertex, normal * distance);
}

inline EdgeHandle findEdgeBetween(const MeshData& mesh, VertexHandle origin, VertexHandle tip) {
    for (EdgeHandle edge : mesh.getEdgeHandles()) {
        if (mesh.getEdgeOrigin(edge) == origin && mesh.getEdgeTip(edge) == tip) return edge;
    }
    return INVALID_EDGE;
}

inline bool everyFaceTriangulates(const MeshData& mesh) {
    for (FaceHandle face : mesh.getFaceHandles()) {
        if (mesh.getFaceTriangles(face).size() != mesh.getFaceVertices(face).size() - 2) return false;
    }
    return true;
}

// Side counts of every face, sorted, e.g. "4444455".
inline std::string faceSides(const MeshData& mesh) {
    std::string sides;
    for (FaceHandle face : mesh.getFaceHandles()) sides += std::to_string(mesh.getFaceVertices(face).size());
    std::sort(sides.begin(), sides.end());
    return sides;
}

inline bool counts(const MeshData& mesh, size_t vertices, size_t halfEdges, size_t faces) {
    return mesh.getVertexHandles().size() == vertices &&
           mesh.getEdgeHandles().size() == halfEdges &&
           mesh.getFaceHandles().size() == faces;
}

// Each edge appears once, counting both halves as the same edge.
inline bool distinctEdges(const MeshData& mesh, const std::vector<EdgeHandle>& edges) {
    for (size_t i = 0; i < edges.size(); ++i) {
        for (size_t j = i + 1; j < edges.size(); ++j) {
            if (edges[i] == edges[j] || mesh.getEdge(edges[i])->pair == edges[j]) return false;
        }
    }
    return true;
}
