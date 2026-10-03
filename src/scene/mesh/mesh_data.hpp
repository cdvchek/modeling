#pragma once

#include <vector>
#include <unordered_map>
#include <types>

#include "scene/mesh/mesh_types.hpp"
#include "scene/mesh/mesh_array.hpp"
#include "core/math/vec2.hpp"

struct VertexData {
    std::vector<f32> vertices;
    std::vector<u32> indexMap;
};

struct EdgeData {
    std::unordered_map<u32, u32> indexMap;
    std::vector<u32> indices;
};

struct FaceData {
    std::vector<u32> indexMap;
    std::vector<u32> indices;
};

class MeshData {
public:
    // ---- Access (mesh_data_access.cpp) ----
    // Look up a single element by handle.

    void setMesh(PresetMesh meshType);

    const Vertex* getVertex(VertexHandle handle) const;
    const Edge* getEdge(EdgeHandle handle) const;
    const Face* getFace(FaceHandle handle) const;

    const std::vector<Vertex> getVertices() const;
    const std::vector<Face> getFaces() const;

    const std::vector<VertexHandle> getVertexHandles() const;
    const std::vector<EdgeHandle> getEdgeHandles() const;
    const std::vector<FaceHandle> getFaceHandles() const;

    VertexHandle getEdgeOrigin(EdgeHandle handle) const;
    VertexHandle getEdgeTip(EdgeHandle handle) const;

    bool isValidHandle(VertexHandle handle) const;
    bool isValidHandle(EdgeHandle handle) const;
    bool isValidHandle(FaceHandle handle) const;

    // ---- Topology queries (mesh_data_queries.cpp) ----
    // Read connectivity. Never modify the mesh.

    std::vector<VertexHandle> getFaceVertices(FaceHandle handle) const;

    // ---- Geometry (mesh_data_geometry.cpp) ----
    // Positions and anything derived from them. Never change connectivity.

    Vec3 getVertexPosition(VertexHandle handle) const;
    Vec3 getFaceNormal(FaceHandle handle) const;
    const std::vector<Triangle>& getFaceTriangles(FaceHandle handle) const;

    void positionVertex(VertexHandle handle, Vec3 position);
    void translateVertex(VertexHandle handle, Vec3 delta);

    void setFacesDirtyByVertex(VertexHandle handle) const;
    void setFacesDirtyByEdge(EdgeHandle handle) const;
    void setFacesDirtyByFace(FaceHandle handle) const;

    // ---- Operators (mesh_data_ops.cpp, mesh_data_merge.cpp) ----
    // Complete edits. The mesh must be valid before and after each call.

    FaceHandle insertFaceRing(FaceHandle handle);
    VertexHandle splitEdge(EdgeHandle handle);

    bool removeVertex(VertexHandle handle);
    bool removeEdge(EdgeHandle handle);
    bool removeFace(FaceHandle handle);

    bool fillFaceLoop(EdgeHandle handle);
    bool connectVertices(VertexHandle a, VertexHandle b);
    bool mergeVertices(VertexHandle a, VertexHandle b, u8 mergeType);

    // ---- GPU export (mesh_data_gpu.cpp) ----
    // Flatten the mesh into buffers for rendering.

    VertexData getVertexData() const;
    EdgeData getEdgeData(const VertexData& vertexData) const;
    FaceData getFaceData(const VertexData& vertexData) const;

private:
    // ---- Topology queries (mesh_data_queries.cpp) ----

    std::vector<EdgeHandle> getFaceEdges(FaceHandle handle) const;
    std::vector<VertexHandle> getVertexNeighbors(VertexHandle handle) const;
    EdgeHandle findEdge(VertexHandle origin, VertexHandle tip) const;
    EdgeHandle findEdgeInFace(FaceHandle face, VertexHandle origin, VertexHandle tip) const;
    EdgeHandle findOutgoingEdge(VertexHandle handle, const std::vector<EdgeHandle>& excluded) const;

    // ---- Geometry (mesh_data_geometry.cpp) ----

    std::vector<Triangle> triangulateFace(FaceHandle handle) const;

    // ---- Primitives (mesh_data_primitives.cpp) ----
    // Low-level pointer edits. The mesh may be left temporarily invalid;
    // operators are responsible for restoring it.

    FaceHandle addFace(const std::vector<VertexHandle>& verts);
    FaceHandle addQuad(VertexHandle v0, VertexHandle v1, VertexHandle v2, VertexHandle v3);
    VertexHandle duplicateVertex(VertexHandle handle);
    void pairEdges(EdgeHandle a, EdgeHandle b);

    void deleteHalfEdge(EdgeHandle handle);
    void deleteFace(FaceHandle handle);
    void deleteFaceWithHalfEdgeLoop(FaceHandle handle);

    MeshArray<Vertex, VertexHandle> m_vertices;
    MeshArray<Edge, EdgeHandle> m_edges;
    MeshArray<Face, FaceHandle> m_faces;
    
    bool m_dirty = true;
};