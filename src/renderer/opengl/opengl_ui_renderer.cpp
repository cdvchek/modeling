#include "renderer/opengl/opengl_ui_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glad/glad.h>
#include "renderer/opengl/opengl_counters.hpp"

void OpenGLUIRenderer::create() {
    destroy();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    const GLsizei stride = sizeof(UIVertex);
    const auto offset = [](std::size_t bytes) { return reinterpret_cast<void*>(bytes); };

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, offset(offsetof(UIVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, offset(offsetof(UIVertex, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, offset(offsetof(UIVertex, local)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, offset(offsetof(UIVertex, halfSize)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, offset(offsetof(UIVertex, radius)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, offset(offsetof(UIVertex, fill)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, offset(offsetof(UIVertex, border)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLUIRenderer::destroy() {
    if (m_ebo != 0) glDeleteBuffers(1, &m_ebo);
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);

    m_vao = m_vbo = m_ebo = 0;
}

void OpenGLUIRenderer::draw(const UIDrawList& list, OpenGLShader& shader, const std::array<OpenGLFont, static_cast<u32>(FontId::Count)>& fonts, u32 viewportWidth, u32 viewportHeight) {
    if (m_vao == 0 || list.getIndices().empty() || viewportWidth == 0 || viewportHeight == 0) return;
    if (!shader.bind()) return;

    // 1. Upload everything at once
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(list.getVertices().size() * sizeof(UIVertex)), list.getVertices().data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(list.getIndices().size() * sizeof(u32)), list.getIndices().data(), GL_STREAM_DRAW);

    // 2. UI state: drawn on top, blended
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.setVec2("u_ViewportSize", Vec2(static_cast<f32>(viewportWidth), static_cast<f32>(viewportHeight)));
    shader.setInt("u_Texture", 0);
    glActiveTexture(GL_TEXTURE0);

    // 3. One draw per batch
    for (const UIDrawBatch& batch : list.getBatches()) {
        if (batch.indexCount == 0) continue;

        if (batch.clipped) {
            // Clip rects are y-down; scissor is y-up from the bottom of the viewport
            const GLint left = static_cast<GLint>(std::floor(batch.clip.x));
            const GLint right = static_cast<GLint>(std::ceil(batch.clip.right()));
            const GLint bottom = static_cast<GLint>(viewportHeight) - static_cast<GLint>(std::ceil(batch.clip.bottom()));
            const GLint top = static_cast<GLint>(viewportHeight) - static_cast<GLint>(std::floor(batch.clip.y));

            glEnable(GL_SCISSOR_TEST);
            glScissor(left, bottom, std::max(0, right - left), std::max(0, top - bottom));
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        if (batch.image != 0) glBindTexture(GL_TEXTURE_2D, batch.image);
        else glBindTexture(GL_TEXTURE_2D, batch.hasTexture ? fonts[static_cast<u32>(batch.texture)].getTexture() : 0);

        drawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.indexCount), GL_UNSIGNED_INT,
                       reinterpret_cast<void*>(static_cast<std::size_t>(batch.indexOffset) * sizeof(u32)));
    }

    // 4. Restore the 3D state
    glDisable(GL_SCISSOR_TEST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
