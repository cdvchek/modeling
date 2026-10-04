#pragma once

#include <types>
#include <vector>
#include <unordered_map>

#include "scene/mesh/mesh_handles.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "renderer/gpu_mesh.hpp"

class OpenGLMesh : public IMesh {
public:
    bool create(const MeshData& mesh);
    bool update(const MeshData& mesh);
    void destroy();

    void drawVertex(VertexHandle handle) const override;
    void drawVertices() const override;
    void drawEdge(EdgeHandle handle) const override;
    void drawEdges() const override;
    void drawFace(FaceHandle handle) const override;
    void drawFaces() const override;

private:
    void bind() const;

    void upload(const MeshData& mesh);

    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_edgeEbo = 0;

    u32 m_faceVao = 0;
    u32 m_faceVbo = 0;
    u32 m_faceEbo = 0;

    u32 m_vCount = 0;
    u32 m_eIndCount = 0;
    u32 m_fIndCount = 0;

    std::vector<u32> m_vertexIndexMap;
    std::unordered_map<u32, u32> m_edgeIndexMap;
    std::vector<u32> m_faceIndexMap;

    bool m_hasIndices = false;
    bool m_initialized = false;
};