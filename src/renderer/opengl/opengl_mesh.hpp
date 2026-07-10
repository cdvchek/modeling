#pragma once

#include <types>

#include "scene/mesh/mesh_data.hpp"
#include "renderer/gpu_mesh.hpp"

class OpenGLMesh : public IMesh {
public:
    bool create(const MeshData& mesh);
    bool update(const MeshData& mesh);
    void draw() const override;
    void destroy();

private:
    void bind() const;

    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_edgeEbo = 0;
    u32 m_faceEbo = 0;

    u32 m_vertexCount = 0;
    u32 m_edgeCount = 0;
    u32 m_faceIndexCount = 0;

    bool m_hasIndices = false;
    bool m_initialized = false;
};