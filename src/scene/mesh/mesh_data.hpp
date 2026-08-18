#pragma once

#include <vector>
#include <unordered_map>
#include <types>

#include "scene/mesh/mesh_types.hpp"
#include "core/math/vec2.hpp"

using HalfEdgeIndex = u32;
using RendererIndex = u32;

struct EdgeData {
    std::unordered_map<HalfEdgeIndex, RendererIndex> indexMap;
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

    Vec3 getVertexPosition(u32 vertexIndex) const;

    std::vector<u32> getFaceVertices(u32 face) const;
    std::vector<u32> getFaceEdges(u32 face) const;
    const std::vector<Triangle>& getFaceTriangles(u32 faceIndex) const;

    void setFacesDirtyByVertex(u32 vertexIndex) const;
    void setFacesDirtyByEdge(u32 edgeIndex) const;
    void setFacesDirtyByFace(u32 faceIndex) const;

    // CPU data manipulation

    void positionVertex(u32 v_index, Vec3 position);
    void positionEdge(u32 e_index, Vec3 position);
    void positionFace(u32 f_index, Vec3 position);

    void translateVertex(u32 v_index, Vec3 delta);
    void translateEdge(u32 e_index, Vec3 delta);
    void translateFace(u32 f_index, Vec3 delta);

    void extrudeFace(u32 f_index, Vec3 delta);

    void scaleVertices(std::vector<u32> v_indices, f32 delta);

    // GPU data access
    std::vector<f32> getVertexData() const;
    EdgeData getEdgeData() const;
    FaceData getFaceData() const;

private:
    std::vector<Triangle> triangulateFace(u32 faceIndex) const;
    std::vector<Vertex> m_vertices;
    std::vector<Edge> m_edges;
    std::vector<Face> m_faces;
    
    bool m_dirty = true;
};