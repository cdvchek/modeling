#include "renderer/opengl/opengl_mesh.hpp"

#include <glad/glad.h>

bool OpenGLMesh::create(const MeshData& mesh) {
    const auto vertices = mesh.getVertexData();
    const auto edges = mesh.getEdgeData();
    
    const FaceData faceData = mesh.getFaceData();
    m_faceIndexMap = faceData.indexMap;
    const auto& faces = faceData.indices;

    if (vertices.empty()) {
        return false;
    }

    m_vCount = static_cast<u32>(vertices.size() / 3);
    m_eIndCount = static_cast<u32>(edges.size());
    m_fIndCount = static_cast<u32>(faces.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_edgeEbo);
    glGenBuffers(1, &m_faceEbo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(f32)),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(3 * sizeof(f32)),
        nullptr
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(edges.size() * sizeof(u32)),
        edges.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(faces.size() * sizeof(u32)),
        faces.data(),
        GL_DYNAMIC_DRAW
    );

    glBindVertexArray(0);

    m_initialized = true;
    return true;
}

bool OpenGLMesh::update(const MeshData& mesh) {
    if (!m_initialized) {
        return create(mesh);
    }

    const auto vertices = mesh.getVertexData();
    const auto edges = mesh.getEdgeData();

    const FaceData faceData = mesh.getFaceData();
    m_faceIndexMap = faceData.indexMap;
    const auto& faces = faceData.indices;

    if (vertices.empty()) {
        return false;
    }

    m_vCount = static_cast<u32>(vertices.size() / 3);
    m_eIndCount = static_cast<u32>(edges.size());
    m_fIndCount = static_cast<u32>(faces.size());

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(f32)),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(edges.size() * sizeof(u32)),
        edges.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(faces.size() * sizeof(u32)),
        faces.data(),
        GL_DYNAMIC_DRAW
    );

    glBindVertexArray(0);

    return true;
}

void OpenGLMesh::drawFace(u32 index) const {
    if (m_vao == 0 || m_fIndCount == 0) {
        return;
    }

    const u32 mapIndex = index * 2;

    if (mapIndex + 1 >= m_faceIndexMap.size()) {
        return;
    }

    const u32 offset = m_faceIndexMap[mapIndex];
    const u32 count  = m_faceIndexMap[mapIndex + 1];

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);

    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(count),
        GL_UNSIGNED_INT,
        reinterpret_cast<void*>(
            static_cast<uintptr_t>(offset * sizeof(u32))
        )
    );
}

void OpenGLMesh::drawFaces() const {
    if (m_vao == 0 || m_fIndCount == 0) {
        return;
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);

    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(m_fIndCount),
        GL_UNSIGNED_INT,
        nullptr
    );
}

void OpenGLMesh::drawEdge(u32 index) const {
    if (m_vao == 0 || m_eIndCount == 0) {
        return;
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);

    glDrawElements(
        GL_LINES,
        static_cast<GLsizei>(2),
        GL_UNSIGNED_INT,
        reinterpret_cast<void*>(
            static_cast<uintptr_t>(index * 2 * sizeof(u32))
        )
    );
}

void OpenGLMesh::drawEdges() const {
    if (m_vao == 0 || m_eIndCount == 0) {
        return;
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);

    glDrawElements(
        GL_LINES,
        static_cast<GLsizei>(m_eIndCount),
        GL_UNSIGNED_INT,
        nullptr
    );
}

void OpenGLMesh::drawVertex(u32 vertexIndex) const {
    if (m_vao == 0 || vertexIndex >= m_vCount) {
        return;
    }

    glBindVertexArray(m_vao);

    glDrawArrays(
        GL_POINTS,
        static_cast<GLint>(vertexIndex),
        1
    );
}

void OpenGLMesh::drawVertices() const {
    if (m_vao == 0 || m_vCount == 0) {
        return;
    }

    glBindVertexArray(m_vao);

    glDrawArrays(
        GL_POINTS,
        0,
        static_cast<GLsizei>(m_vCount)
    );
}

void OpenGLMesh::bind() const {
    glBindVertexArray(m_vao);
}

void OpenGLMesh::destroy() {
    if (m_edgeEbo != 0) {
        glDeleteBuffers(1, &m_edgeEbo);
        m_edgeEbo = 0;
    }

    if (m_faceEbo != 0) {
        glDeleteBuffers(1, &m_faceEbo);
        m_faceEbo = 0;
    }

    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_vCount = 0;
    m_eIndCount = 0;
    m_fIndCount = 0;
    m_hasIndices = false;
    m_initialized = false;
}