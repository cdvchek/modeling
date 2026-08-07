#pragma once

#include "scene/mesh/mesh_types.hpp"
#include "core/math/vec2.hpp"

class MeshData {
public:
    void setMesh(PresetMesh meshType);
    const std::vector<Vertex> getVertices() const;
    const std::vector<Face> getFaces() const;

    std::vector<u32> getFaceVertices(u32 face) const;
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
    std::vector<u32> getEdgeData() const;
    std::vector<u32> getFaceData() const;

private:
    std::vector<Triangle> triangulateFace(u32 faceIndex) const;
    std::vector<Vertex> m_vertices;
    std::vector<Edge> m_edges;
    std::vector<Face> m_faces;
    
    bool m_dirty = true;
};