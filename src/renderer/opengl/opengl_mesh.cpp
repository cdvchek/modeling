#include "renderer/opengl/opengl_mesh.hpp"

#include <glad/glad.h>

bool OpenGLMesh::create(const MeshData& mesh) {
    const auto& vertices = mesh.getVertices();
    const auto& edges = mesh.getEdges();
    const auto& faces = mesh.getFaces();

    if (vertices.empty()) return false;

    m_vertexCount = static_cast<u32>(vertices.size());
    m_edgeCount = static_cast<u32>(edges.size());

    m_faceIndexCount = 0;
    std::vector<u32> gpuFaceIndices;
    for(auto& face : faces) {
        m_faceIndexCount += (u32)face.vertices.size();
        gpuFaceIndices.insert(gpuFaceIndices.end(), face.vertices.begin(), face.vertices.end());
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_edgeEbo);
    glGenBuffers(1, &m_faceEbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);
    
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        edges.size() * sizeof(Edge),
        edges.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        m_faceIndexCount * sizeof(u32),
        gpuFaceIndices.data(),
        GL_DYNAMIC_DRAW
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

    m_initialized = true;
    return true;
}

bool OpenGLMesh::update(const MeshData& mesh) {
    if (!m_initialized) {
        return create(mesh);
    }

    const auto& vertices = mesh.getVertices();
    if (vertices.empty()) return false;

    const auto& edges = mesh.getEdges();
    const auto& faces = mesh.getFaces();

    m_vertexCount = static_cast<u32>(vertices.size());
    m_edgeCount = static_cast<u32>(edges.size());
    
    m_faceIndexCount = 0;
    std::vector<u32> gpuFaceIndices;
    for(auto& face : faces) {
        m_faceIndexCount += (u32)face.vertices.size();
        gpuFaceIndices.insert(gpuFaceIndices.end(), face.vertices.begin(), face.vertices.end());
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        edges.size() * sizeof(Edge),
        edges.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        m_faceIndexCount * sizeof(u32),
        gpuFaceIndices.data(),
        GL_DYNAMIC_DRAW
    );

    return true;
}

void OpenGLMesh::draw() const {
    if (m_vao == 0 || m_vertexCount == 0) return;

    bind();

    glDrawArrays(
        GL_POINTS,
        0,
        static_cast<GLsizei>(m_vertexCount)
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);
    glDrawElements(
        GL_LINES,
        m_edgeCount * 2,
        GL_UNSIGNED_INT,
        0
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);
    glDrawElements(
        GL_TRIANGLES,
        m_faceIndexCount,
        GL_UNSIGNED_INT,
        nullptr
    );

    glBindVertexArray(0);
}

void OpenGLMesh::bind() const {
    glBindVertexArray(m_vao);
}

void OpenGLMesh::destroy() {
    if (m_edgeEbo != 0) {
        glDeleteBuffers(1, &m_edgeEbo);
        m_edgeEbo = 0;
    }

    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_vertexCount = 0;
    m_edgeCount = 0;
    m_faceIndexCount = 0;
    m_hasIndices = false;
    m_initialized = false;
}