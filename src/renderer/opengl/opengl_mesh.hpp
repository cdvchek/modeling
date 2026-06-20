#pragma once

#include <types>

#include "scene/mesh/mesh_data.hpp"
#include "renderer/gpu_mesh.hpp"

class OpenGLMesh : public IMesh {
public:
    bool create(const MeshData& mesh);
    bool create(const Vertex* vertices, u32 vertex_count, const u32* indices, u32 index_count);
    void draw() const override;
    void destroy();

private:
    void bind() const;

    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_ebo = 0;
    u32 m_indexCount = 0;
};