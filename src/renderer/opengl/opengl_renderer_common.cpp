#include "renderer/opengl/opengl_renderer.hpp"
#include "core/input/contexts.hpp"
#include "core/font/bitmap_font.hpp"

#include <glad/glad.h>

OpenGLRenderer::~OpenGLRenderer() { shutdown(); }

void OpenGLRenderer::beginFrame(){
    if (!m_initialized) return;
}

void OpenGLRenderer::beginMainPass(const ClearState& clearState){
    if (!m_initialized) return;

    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));

    GLbitfield clearMask = 0;

    if (clearState.clearColor) {
        glClearColor(clearState.r, clearState.g, clearState.b, clearState.a);
        clearMask |= GL_COLOR_BUFFER_BIT;
    }

    if (clearState.clearDepth) {
        glClearDepth(clearState.depth);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (clearState.clearStencil) {
        glClearStencil(clearState.stencil);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }

    if (clearMask != 0) glClear(clearMask);
}

void OpenGLRenderer::draw(const DrawCommand& command) {
    if (!m_initialized || !command.mesh) return;

    m_testShader->bind();
    m_testShader->setMat4("u_MVP", command.mvp.m);

    // Faces
    if (command.showFaces) {
        m_testShader->setVec3("u_Color", Vec3(1.0f, 1.0f, 0.0f));

        for (const FaceHandle& face : command.highlightedFaces) {
            command.mesh->drawFace(face);
        }

        m_testShader->setVec3("u_Color", Vec3(0.7f, 0.7f, 0.7f));

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);

        command.mesh->drawFaces();

        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Edges
    if (command.showEdges) {
        m_testShader->setVec3("u_Color", Vec3(1.0f, 1.0f, 0.0f));

        for (const EdgeHandle& edge : command.highlightedEdges) {
            command.mesh->drawEdge(edge);
        }

        m_testShader->setVec3("u_Color", Vec3(0.0f, 0.0f, 0.0f));

        glLineWidth(2.0f);
        command.mesh->drawEdges();
    }

    // Vertices
    if (command.showVerts) {
        m_testShader->setVec3("u_Color", Vec3(1.0f, 1.0f, 0.0f));

        for (const VertexHandle& vertex : command.highlightedVerts) {
            command.mesh->drawVertex(vertex);
        }

        m_testShader->setVec3("u_Color", Vec3(0.0f, 0.0f, 0.0f));

        glPointSize(8.0f);
        command.mesh->drawVertices();
    }

    glBindVertexArray(0);
}

void OpenGLRenderer::drawText(const std::string& text, f32 x, f32 y) {
    if (text.empty()) return;

    constexpr f32 charWidth = 16.0f;
    constexpr f32 charHeight = 24.0f;

    std::vector<f32> vertices;
    vertices.reserve(text.size() * 6 * 4);

    f32 currentX = x;
    f32 currentY = y;

    for (char character : text) {
        if (character == '\n') {
            currentX = x;
            currentY += charHeight;
            continue;
        }

        GlyphUV uv = m_consoleFont.getGlyphUV(character);

        f32 left =
            (currentX / static_cast<f32>(m_width)) * 2.0f - 1.0f;

        f32 right =
            ((currentX + charWidth) / static_cast<f32>(m_width)) * 2.0f - 1.0f;

        f32 top =
            1.0f - (currentY / static_cast<f32>(m_height)) * 2.0f;

        f32 bottom =
            1.0f - ((currentY + charHeight) / static_cast<f32>(m_height)) * 2.0f;

        f32 characterVertices[] = {
            left,  top,       uv.u0, uv.v0,
            left,  bottom,    uv.u0, uv.v1,
            right, bottom,    uv.u1, uv.v1,

            left,  top,       uv.u0, uv.v0,
            right, bottom,    uv.u1, uv.v1,
            right, top,       uv.u1, uv.v0
        };

        vertices.insert(
            vertices.end(),
            std::begin(characterVertices),
            std::end(characterVertices)
        );

        currentX += charWidth;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(f32),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_textShader->bind();

    m_textShader->setVec3("uColor", {1.0f, 1.0f, 1.0f});

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(
        GL_TEXTURE_2D,
        m_consoleFont.getTexture()
    );

    glBindVertexArray(m_textVAO);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(vertices.size() / 4)
    );

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderer::drawDebugLine(
    const Vec3& start,
    const Vec3& end
) {
    const float vertices[] = {
        start.x, start.y, start.z,
        end.x,   end.y,   end.z
    };

    glBindVertexArray(m_debugLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugLineVBO);

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(vertices),
        vertices
    );

    glDrawArrays(GL_LINES, 0, 2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OpenGLRenderer::drawConsoleBackground() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_consoleShader->bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderer::endMainPass(){
    if (!m_initialized) return;
}

void OpenGLRenderer::endFrame(){
    if (!m_initialized) return;
}

void OpenGLRenderer::resize(u32 width, u32 height){
    m_width = width;
    m_height = height;

    if (!m_initialized) return;

    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

RendererBackend OpenGLRenderer::getBackend() const{
    return RendererBackend::RB_OpenGL;
}

const char* OpenGLRenderer::getBackendName() const{
    return "OpenGL";
}

void OpenGLRenderer::drawCharacter(
    const OpenGLFont& font,
    char character,
    float x,
    float y
) {

    // Get this character's location in the font atlas.
    GlyphUV uv = font.getGlyphUV(character);

    float width = 16.0f;
    float height = 24.0f;

    float left = (x / static_cast<float>(m_width)) * 2.0f - 1.0f;

    float right = ((x + width) / static_cast<float>(m_width)) * 2.0f - 1.0f;

    float top = 1.0f - (y / static_cast<float>(m_height)) * 2.0f;

    float bottom = 1.0f - ((y + height) / static_cast<float>(m_height)) * 2.0f;

    float vertices[] = {

        // position       // texture

        left,  top,       uv.u0, uv.v0,
        left,  bottom,    uv.u0, uv.v1,
        right, bottom,    uv.u1, uv.v1,

        left,  top,       uv.u0, uv.v0,
        right, bottom,    uv.u1, uv.v1,
        right, top,       uv.u1, uv.v0
    };


    // Update our text VBO with this character.

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_textVBO
    );

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(vertices),
        vertices
    );


    // Text should render over the scene/console.

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );


    // Use text shader.

    m_textShader->bind();


    // Bind font atlas.

    glActiveTexture(GL_TEXTURE0);

    glBindTexture(
        GL_TEXTURE_2D,
        font.getTexture()
    );


    // Draw character.

    glBindVertexArray(m_textVAO);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );


    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);


    glEnable(GL_DEPTH_TEST);
}
