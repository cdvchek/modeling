#pragma once

#include <types>

struct Vertex {
    f32 x, y, z;
};

class OpenGLMesh {
public:
    bool create(const Vertex* vertices, u32 vertex_count, const u32* indices, u32 index_count);
    void draw() const;
    void destroy();
    
private:
    void bind() const;

    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_ebo = 0;
    u32 m_indexCount = 0;
};