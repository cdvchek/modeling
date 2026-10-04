#include "renderer/opengl/opengl_mesh.hpp"

#include <glad/glad.h>

bool OpenGLMesh::create(const MeshData& mesh) {
    destroy();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_edgeEbo);

    glGenVertexArrays(1, &m_faceVao);
    glGenBuffers(1, &m_faceVbo);
    glGenBuffers(1, &m_faceEbo);

    // Shared positions for edges and points
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(3 * sizeof(f32)), nullptr);

    // Per-face corners: position + normal
    const GLsizei faceStride = static_cast<GLsizei>(FaceData::FLOATS_PER_VERTEX * sizeof(f32));

    glBindVertexArray(m_faceVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_faceVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, faceStride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, faceStride, reinterpret_cast<void*>(3 * sizeof(f32)));

    glBindVertexArray(0);

    m_initialized = true;

    upload(mesh);
    return m_vCount > 0;
}

bool OpenGLMesh::update(const MeshData& mesh) {
    if (!m_initialized) {
        return create(mesh);
    }

    upload(mesh);
    return m_vCount > 0;
}

void OpenGLMesh::upload(const MeshData& mesh) {
    const VertexData vertexData = mesh.getVertexData();
    const EdgeData edgeData = mesh.getEdgeData(vertexData);
    const FaceData faceData = mesh.getFaceData();

    m_vertexIndexMap = vertexData.indexMap;
    m_edgeIndexMap = edgeData.indexMap;
    m_faceIndexMap = faceData.indexMap;

    m_vCount = static_cast<u32>(vertexData.vertices.size() / 3);
    m_eIndCount = static_cast<u32>(edgeData.indices.size());
    m_fIndCount = static_cast<u32>(faceData.indices.size());

    // Element buffers bind to the VAO, so bind the VAO before uploading each one
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexData.vertices.size() * sizeof(f32)), vertexData.vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(edgeData.indices.size() * sizeof(u32)), edgeData.indices.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(m_faceVao);

    glBindBuffer(GL_ARRAY_BUFFER, m_faceVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(faceData.vertices.size() * sizeof(f32)), faceData.vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_faceEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(faceData.indices.size() * sizeof(u32)), faceData.indices.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLMesh::drawFace(FaceHandle handle) const {
    if (m_vao == 0 || m_fIndCount == 0) return;

    const u32 mapIndex = handle.index * 2;

    if (mapIndex + 1 >= m_faceIndexMap.size()) return;

    const u32 offset = m_faceIndexMap[mapIndex];
    const u32 count = m_faceIndexMap[mapIndex + 1];

    if (offset == INVALID_INDEX || count == INVALID_INDEX) return;

    glBindVertexArray(m_faceVao);

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

    glBindVertexArray(m_faceVao);

    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(m_fIndCount),
        GL_UNSIGNED_INT,
        nullptr
    );
}

void OpenGLMesh::drawEdge(EdgeHandle handle) const {
    if (m_vao == 0 || m_eIndCount == 0) return;

    auto it = m_edgeIndexMap.find(handle.index);
    if (it == m_edgeIndexMap.end()) return;

    const u32 offset = it->second;

    glBindVertexArray(m_vao);

    glDrawElements(
        GL_LINES,
        2,
        GL_UNSIGNED_INT,
        reinterpret_cast<void*>(
            static_cast<uintptr_t>(offset * sizeof(u32))
        )
    );
}

void OpenGLMesh::drawEdges() const {
    if (m_vao == 0 || m_eIndCount == 0) {
        return;
    }

    glBindVertexArray(m_vao);

    glDrawElements(
        GL_LINES,
        static_cast<GLsizei>(m_eIndCount),
        GL_UNSIGNED_INT,
        nullptr
    );
}

void OpenGLMesh::drawVertex(VertexHandle handle) const {
    if (m_vao == 0) return;
    if (handle.index >= m_vertexIndexMap.size()) return;

    const u32 renderIndex = m_vertexIndexMap[handle.index];

    if (renderIndex == INVALID_INDEX) return;

    glBindVertexArray(m_vao);

    glDrawArrays(
        GL_POINTS,
        static_cast<GLint>(renderIndex),
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

    if (m_faceVbo != 0) {
        glDeleteBuffers(1, &m_faceVbo);
        m_faceVbo = 0;
    }

    if (m_faceVao != 0) {
        glDeleteVertexArrays(1, &m_faceVao);
        m_faceVao = 0;
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