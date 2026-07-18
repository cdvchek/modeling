#pragma once

#include "scene/mesh/mesh_types.hpp"
#include "core/math/vec2.hpp"

class MeshData {
public:
    void setMesh(PresetMesh meshType);

    // CPU data manipulation

    void positionVertex(u32 v_index, Vec3 position);
    void positionEdge(u32 e_index, Vec3 position);
    void positionFace(u32 f_index, Vec3 position);

    void translateVertex(u32 v_index, Vec3 delta);
    void translateEdge(u32 e_index, Vec3 delta);
    void translateFace(u32 f_index, Vec3 delta);

    void extrudeEdge(u32 e_index, Vec3 delta);
    void extrudeFace(u32 f_index, Vec3 delta);

    // GPU data access

    const std::vector<f32> getVertexData() const;
    const std::vector<u32> getEdgeData() const;
    const std::vector<u32> getFaceData() const;

private:
    std::vector<Vertex> m_vertices;
    std::vector<Edge> m_edges;
    std::vector<Face> m_faces;
    
    bool m_dirty = true;
};