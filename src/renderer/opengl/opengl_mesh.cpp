#include "renderer/opengl/opengl_mesh.hpp"

#include <glad/glad.h>

bool OpenGLMesh::create(const MeshData& mesh) {
    return create(
        mesh.vertices.data(),
        static_cast<u32>(mesh.vertices.size()),
        mesh.indices.data(),
        static_cast<u32>(mesh.indices.size())
    );
}

bool OpenGLMesh::create(const Vertex* vertices, u32 vertex_count, const u32* indices, u32 index_count) {
    if (!vertices || vertex_count == 0) return false;
    if (!indices || index_count == 0) return false;

    m_indexCount = index_count;

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertex_count * sizeof(Vertex),
        vertices,
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        index_count * sizeof(u32),
        indices,
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, position))
    );

    glBindVertexArray(0);

    return true;
}

void OpenGLMesh::draw() const {
    if (m_vao == 0 || m_indexCount == 0) return;

    bind();

    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(m_indexCount),
        GL_UNSIGNED_INT,
        nullptr
    );

    glBindVertexArray(0);
}

void OpenGLMesh::bind() const {
    glBindVertexArray(m_vao);
}

void OpenGLMesh::destroy() {
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }

    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_indexCount = 0;
}