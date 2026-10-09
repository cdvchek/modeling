#include "renderer/opengl/opengl_mesh.hpp"

#include <algorithm>

#include <glad/glad.h>
#include "renderer/opengl/opengl_counters.hpp"

namespace {
    // A set's identity: each handle's index and generation, so a changed selection (or a reused slot) rebuilds it
    template <typename H>
    std::vector<u32> keyOf(const std::vector<H>& handles) {
        std::vector<u32> key;
        key.reserve(handles.size() * 2);
        for (const H& handle : handles) key.insert(key.end(), { handle.index, handle.generation });
        return key;
    }
}

bool OpenGLMesh::create(const MeshData& mesh, const FaceGroupOf& groupOf, u64 groupingStamp) {
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
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, faceStride, reinterpret_cast<void*>(6 * sizeof(f32)));

    glBindVertexArray(0);

    createSet(m_vertexSet, m_vbo, false);
    createSet(m_edgeSet, m_vbo, false);
    createSet(m_hardEdgeSet, m_vbo, false);
    createSet(m_faceSet, m_faceVbo, true);

    m_initialized = true;

    upload(mesh, groupOf, groupingStamp);
    return m_vCount > 0;
}

bool OpenGLMesh::update(const MeshData& mesh, const FaceGroupOf& groupOf, u64 groupingStamp) {
    if (!m_initialized) {
        return create(mesh, groupOf, groupingStamp);
    }

    upload(mesh, groupOf, groupingStamp);
    return m_vCount > 0;
}

bool OpenGLMesh::patch(const MeshData& mesh, u64 groupingStamp) {
    if (!m_initialized || !(mesh.stamp() == m_stamp) || groupingStamp != m_groupingStamp) return false;

    std::vector<VertexHandle> moved = mesh.allMoved() ? mesh.getVertexHandles() : mesh.movedVertices();
    if (moved.empty()) return true;
    std::sort(moved.begin(), moved.end(), [](VertexHandle a, VertexHandle b) { return a.index < b.index; });
    moved.erase(std::unique(moved.begin(), moved.end()), moved.end());

    // Positions for points and edges, keeping the lowest and highest float changed
    std::size_t low = m_positions.size(), high = 0;
    for (VertexHandle vertex : moved) {
        if (vertex.index >= m_vertexIndexMap.size() || m_vertexIndexMap[vertex.index] == INVALID_INDEX) return false;
        const std::size_t at = static_cast<std::size_t>(m_vertexIndexMap[vertex.index]) * 3;
        if (at + 3 > m_positions.size()) return false;
        const Vec3 position = mesh.getVertexPosition(vertex);
        m_positions[at] = position.x;
        m_positions[at + 1] = position.y;
        m_positions[at + 2] = position.z;
        low = std::min(low, at);
        high = std::max(high, at + 3);
    }

    // Every face whose corners changed gets them again, in its same place (a face keeps its triangle count)
    const std::vector<FaceHandle> faces = mesh.getFacesToPatch(moved);

    const std::size_t stride = FaceData::FLOATS_PER_VERTEX;
    std::size_t faceLow = m_faceVertices.size(), faceHigh = 0;
    std::vector<f32> corners;
    for (FaceHandle face : faces) {
        const u32 mapIndex = face.index * 2;
        if (mapIndex + 1 >= m_faceIndexMap.size() || m_faceIndexMap[mapIndex] == INVALID_INDEX) return false;
        const std::size_t first = static_cast<std::size_t>(m_faceIndexMap[mapIndex]) * stride;
        const std::size_t count = static_cast<std::size_t>(m_faceIndexMap[mapIndex + 1]) * stride;

        corners.clear();
        mesh.appendFaceCorners(face, corners);
        if (corners.size() != count || first + count > m_faceVertices.size()) return false;
        std::copy(corners.begin(), corners.end(), m_faceVertices.begin() + static_cast<std::ptrdiff_t>(first));
        faceLow = std::min(faceLow, first);
        faceHigh = std::max(faceHigh, first + count);
    }

    // One upload per buffer, covering everything that changed
    RenderStats& counters = glCounters();
    ++counters.meshPatches;
    if (high > low) {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(low * sizeof(f32)), static_cast<GLsizeiptr>((high - low) * sizeof(f32)), m_positions.data() + low);
        counters.meshPatchBytes += (high - low) * sizeof(f32);
    }
    if (faceHigh > faceLow) {
        glBindBuffer(GL_ARRAY_BUFFER, m_faceVbo);
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(faceLow * sizeof(f32)), static_cast<GLsizeiptr>((faceHigh - faceLow) * sizeof(f32)), m_faceVertices.data() + faceLow);
        counters.meshPatchBytes += (faceHigh - faceLow) * sizeof(f32);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void OpenGLMesh::createSet(ElementSet& set, u32 vbo, bool faceLayout) {
    glGenVertexArrays(1, &set.vao);
    glGenBuffers(1, &set.ebo);

    glBindVertexArray(set.vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, set.ebo);

    if (faceLayout) {
        const GLsizei stride = static_cast<GLsizei>(FaceData::FLOATS_PER_VERTEX * sizeof(f32));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(f32)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(f32)));
    } else {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(3 * sizeof(f32)), nullptr);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLMesh::destroySet(ElementSet& set) {
    if (set.ebo != 0) glDeleteBuffers(1, &set.ebo);
    if (set.vao != 0) glDeleteVertexArrays(1, &set.vao);
    set = {};
}

bool OpenGLMesh::prepareSet(ElementSet& set, const std::vector<u32>& key, const std::vector<u32>& indices) {
    if (!set.built || set.key != key) {
        glBindVertexArray(set.vao);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(u32)), indices.data(), GL_DYNAMIC_DRAW);
        set.count = static_cast<u32>(indices.size());
        set.key = key;
        set.built = true;
    }

    if (set.count == 0) return false;
    glBindVertexArray(set.vao);
    return true;
}

void OpenGLMesh::upload(const MeshData& mesh, const FaceGroupOf& groupOf, u64 groupingStamp) {
    const VertexData vertexData = mesh.getVertexData();
    const EdgeData edgeData = mesh.getEdgeData(vertexData);
    const FaceData faceData = mesh.getFaceData(groupOf);

    RenderStats& counters = glCounters();
    ++counters.meshUploads;
    counters.meshUploadBytes += (vertexData.vertices.size() + faceData.vertices.size()) * sizeof(f32)
                              + (edgeData.indices.size() + faceData.indices.size()) * sizeof(u32);

    m_vertexIndexMap = vertexData.indexMap;
    m_edgeIndexMap = edgeData.indexMap;
    m_edgeIndices = edgeData.indices;
    m_faceIndexMap = faceData.indexMap;
    m_faceGroups = faceData.groups;
    m_stamp = mesh.stamp();
    m_groupingStamp = groupingStamp;
    m_positions = vertexData.vertices;
    m_faceVertices = faceData.vertices;

    m_vCount = static_cast<u32>(vertexData.vertices.size() / 3);
    m_eIndCount = static_cast<u32>(edgeData.indices.size());
    m_fIndCount = static_cast<u32>(faceData.indices.size());

    // The element maps changed, so every selection set is rebuilt on its next draw
    m_vertexSet.built = m_edgeSet.built = m_hardEdgeSet.built = m_faceSet.built = false;

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

void OpenGLMesh::drawFaces() const {
    if (m_vao == 0 || m_fIndCount == 0) return;

    glBindVertexArray(m_faceVao);
    drawElements(GL_TRIANGLES, static_cast<GLsizei>(m_fIndCount), GL_UNSIGNED_INT, nullptr);
}

void OpenGLMesh::drawFaceRange(u32 firstIndex, u32 indexCount) const {
    if (m_vao == 0 || indexCount == 0 || firstIndex + indexCount > m_fIndCount) return;

    glBindVertexArray(m_faceVao);
    drawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT,
                 reinterpret_cast<void*>(static_cast<uintptr_t>(firstIndex) * sizeof(u32)));
}

void OpenGLMesh::drawEdges() const {
    if (m_vao == 0 || m_eIndCount == 0) return;

    glBindVertexArray(m_vao);
    drawElements(GL_LINES, static_cast<GLsizei>(m_eIndCount), GL_UNSIGNED_INT, nullptr);
}

void OpenGLMesh::drawVertices() const {
    if (m_vao == 0 || m_vCount == 0) return;

    glBindVertexArray(m_vao);
    drawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_vCount));
}

void OpenGLMesh::drawVertexSet(const std::vector<VertexHandle>& vertices) {
    if (m_vao == 0) return;

    const std::vector<u32> key = keyOf(vertices);
    std::vector<u32> indices;
    if (!m_vertexSet.built || m_vertexSet.key != key) {
        for (VertexHandle handle : vertices) {
            if (handle.index < m_vertexIndexMap.size() && m_vertexIndexMap[handle.index] != INVALID_INDEX) {
                indices.push_back(m_vertexIndexMap[handle.index]);
            }
        }
    }

    if (prepareSet(m_vertexSet, key, indices)) drawElements(GL_POINTS, static_cast<GLsizei>(m_vertexSet.count), GL_UNSIGNED_INT, nullptr);
}

void OpenGLMesh::drawEdgeSet(const std::vector<EdgeHandle>& edges) {
    drawEdgesIn(m_edgeSet, edges);
}

void OpenGLMesh::drawHardEdgeSet(const std::vector<EdgeHandle>& edges) {
    drawEdgesIn(m_hardEdgeSet, edges);
}

void OpenGLMesh::drawEdgesIn(ElementSet& set, const std::vector<EdgeHandle>& edges) {
    if (m_vao == 0) return;

    const std::vector<u32> key = keyOf(edges);
    std::vector<u32> indices;
    if (!set.built || set.key != key) {
        for (EdgeHandle handle : edges) {
            const auto found = m_edgeIndexMap.find(handle.index);
            if (found == m_edgeIndexMap.end() || found->second + 1 >= m_edgeIndices.size()) continue;
            indices.insert(indices.end(), { m_edgeIndices[found->second], m_edgeIndices[found->second + 1] });
        }
    }

    if (prepareSet(set, key, indices)) drawElements(GL_LINES, static_cast<GLsizei>(set.count), GL_UNSIGNED_INT, nullptr);
}

void OpenGLMesh::drawFaceSet(const std::vector<FaceHandle>& faces) {
    if (m_vao == 0) return;

    // Each corner of a face has its own vertex, numbered in index order, so a face's indices are a plain run
    const std::vector<u32> key = keyOf(faces);
    std::vector<u32> indices;
    if (!m_faceSet.built || m_faceSet.key != key) {
        for (FaceHandle handle : faces) {
            const u32 mapIndex = handle.index * 2;
            if (mapIndex + 1 >= m_faceIndexMap.size()) continue;
            const u32 first = m_faceIndexMap[mapIndex];
            const u32 count = m_faceIndexMap[mapIndex + 1];
            if (first == INVALID_INDEX || count == INVALID_INDEX) continue;
            for (u32 k = 0; k < count; ++k) indices.push_back(first + k);
        }
    }

    if (prepareSet(m_faceSet, key, indices)) drawElements(GL_TRIANGLES, static_cast<GLsizei>(m_faceSet.count), GL_UNSIGNED_INT, nullptr);
}

void OpenGLMesh::destroy() {
    destroySet(m_vertexSet);
    destroySet(m_edgeSet);
    destroySet(m_hardEdgeSet);
    destroySet(m_faceSet);

    if (m_edgeEbo != 0) glDeleteBuffers(1, &m_edgeEbo);
    if (m_faceEbo != 0) glDeleteBuffers(1, &m_faceEbo);
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_faceVbo != 0) glDeleteBuffers(1, &m_faceVbo);
    if (m_faceVao != 0) glDeleteVertexArrays(1, &m_faceVao);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);

    m_edgeEbo = m_faceEbo = m_vbo = m_faceVbo = m_faceVao = m_vao = 0;
    m_vCount = 0;
    m_eIndCount = 0;
    m_fIndCount = 0;
    m_faceGroups.clear();
    m_initialized = false;
}
