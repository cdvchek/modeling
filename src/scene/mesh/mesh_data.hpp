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
    void setMesh(PresetMesh meshType);
    const std::vector<Vertex> getVertices() const;
    const std::vector<Face> getFaces() const;

    const std::vector<VertexHandle> getVertexHandles() const;
    const std::vector<EdgeHandle> getEdgeHandles() const;
    const std::vector<FaceHandle> getFaceHandles() const;

    Vec3 getVertexPosition(VertexHandle handle) const;

    std::vector<VertexHandle> getFaceVertices(FaceHandle handle) const;
    // std::vector<u32> getFaceEdges(u32 face) const;
    const std::vector<Triangle>& getFaceTriangles(FaceHandle handle) const;

    void setFacesDirtyByVertex(VertexHandle handle) const;
    void setFacesDirtyByEdge(EdgeHandle handle) const;
    void setFacesDirtyByFace(FaceHandle handlehandle) const;

    // CPU data manipulation

    void positionVertex(VertexHandle handle, Vec3 position);
    void translateVertex(VertexHandle handle, Vec3 delta);
    void scaleVertices(std::vector<VertexHandle> handles, f32 delta);

    FaceHandle insertFaceRing(FaceHandle handle);
    VertexHandle splitEdge(EdgeHandle handle);

    // GPU data access
    VertexData getVertexData() const;
    EdgeData getEdgeData(const VertexData& vertexData) const;
    FaceData getFaceData(const VertexData& vertexData) const;

    VertexHandle getEdgeOrigin(EdgeHandle handle) const;
    VertexHandle getEdgeTip(EdgeHandle handle) const;
    
    bool isValidHandle(VertexHandle handle) const;
    bool isValidHandle(EdgeHandle handle) const;
    bool isValidHandle(FaceHandle handle) const;
private:
    std::vector<Triangle> triangulateFace(FaceHandle handle) const;

    void deleteFaceWithHalfEdgeLoop(FaceHandle handle);
    VertexHandle duplicateVertex(VertexHandle handle);
    FaceHandle addQuad(VertexHandle v0, VertexHandle v1, VertexHandle v2, VertexHandle v3);
    FaceHandle addFace(const std::vector<VertexHandle>& verts);
    EdgeHandle findEdge(VertexHandle origin, VertexHandle tip) const;
    EdgeHandle findEdgeInFace(FaceHandle face, VertexHandle origin, VertexHandle tip) const;
    void pairEdges(EdgeHandle a, EdgeHandle b);
    std::vector<EdgeHandle> getFaceEdges(FaceHandle handle) const;
    EdgeHandle findOutgoingEdge(VertexHandle handle, const std::vector<EdgeHandle>& excluded) const;
    void deleteHalfEdge(EdgeHandle handle);
    void deleteFace(FaceHandle face);

    MeshArray<Vertex, VertexHandle> m_vertices;
    MeshArray<Edge, EdgeHandle> m_edges;
    MeshArray<Face, FaceHandle> m_faces;
    
    bool m_dirty = true;
};