#include "renderer/opengl/opengl_renderer.hpp"
#include "core/input/contexts.hpp"
#include "core/font/font_atlas.hpp"

#include <iostream>
#include <algorithm>

#include <glad/glad.h>

namespace {
    // Dracula-themed: a cool gray surface, edges in the theme's darker background
    const Vec3 FACE_COLOR { 0.72f, 0.73f, 0.78f };
    const Vec3 EDGE_COLOR { 0.13f, 0.13f, 0.17f };
    const Vec3 VERTEX_COLOR { 0.10f, 0.10f, 0.13f };
    const Vec3 OUTLINE_COLOR { 0.06f, 0.06f, 0.08f };

    // Selection matches the light markers and UI accent: Dracula purple with a soft glow
    const Vec3 SELECTED_COLOR { 0.74f, 0.58f, 0.98f };
    const Vec3 SELECTED_FACE_COLOR { 0.46f, 0.34f, 0.74f };
    constexpr f32 SELECTED_GLOW_ALPHA = 0.35f;

    constexpr f32 EDGE_WIDTH = 2.0f;
    constexpr f32 SELECTED_EDGE_WIDTH = 2.5f;
    constexpr f32 SELECTED_EDGE_GLOW_WIDTH = 9.0f;
    constexpr f32 SELECTED_EDGE_INNER_GLOW_WIDTH = 5.0f;
    constexpr f32 VERTEX_DEPTH_BIAS = 0.0005f;

    constexpr f32 VERTEX_SIZE = 7.0f;
    constexpr f32 SELECTED_VERTEX_SIZE = 8.0f;
    constexpr f32 SELECTED_VERTEX_OUTLINE_SIZE = 11.0f;
    constexpr f32 SELECTED_VERTEX_GLOW_SIZE = 22.0f;

    // Point shapes understood by unlit.frag
    constexpr i32 POINT_SQUARE = 0;
    constexpr i32 POINT_DISC = 1;
    constexpr i32 POINT_GLOW = 2;

    // Compatibility contexts need point sprites enabled for gl_PointCoord; GLAD's core header doesn't define it
    constexpr GLenum GL_POINT_SPRITE = 0x8861;
}

OpenGLRenderer::~OpenGLRenderer() { shutdown(); }

bool OpenGLRenderer::createResources() {
    glEnable(GL_DEPTH_TEST);

    // Debug lines: two 3D points
    glGenVertexArrays(1, &m_debugLineVAO);
    glGenBuffers(1, &m_debugLineVBO);

    glBindVertexArray(m_debugLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugLineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 6, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (void*)0);

    // World text: 3D position + UV
    glGenVertexArrays(1, &m_text3DVAO);
    glGenBuffers(1, &m_text3DVBO);

    glBindVertexArray(m_text3DVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_text3DVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 6 * 5, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)(3 * sizeof(f32)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Fullscreen passes take positions from gl_VertexID, so this VAO stays empty
    glGenVertexArrays(1, &m_fullscreenVAO);

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    m_msaaSamples = std::min<i32>(m_msaaSamples, maxSamples);

    createRenderTargets(m_width, m_height);

    if (!m_shaders.loadAll()) {
        std::cerr << "[renderer] some shaders failed to load; their draws will be skipped" << std::endl;
    }

    m_ui.create();

    return true;
}

void OpenGLRenderer::destroyResources() {
    m_shaders.destroy();
    destroyRenderTargets();
    m_ui.destroy();

    for (OpenGLFont& font : m_fonts) font.destroy();

    glDeleteVertexArrays(1, &m_text3DVAO);
    glDeleteBuffers(1, &m_text3DVBO);
    glDeleteVertexArrays(1, &m_debugLineVAO);
    glDeleteBuffers(1, &m_debugLineVBO);
    glDeleteVertexArrays(1, &m_fullscreenVAO);

    m_text3DVAO = m_text3DVBO = 0;
    m_debugLineVAO = m_debugLineVBO = 0;
    m_fullscreenVAO = 0;
}

bool OpenGLRenderer::loadFonts(const FontLibrary& fonts) {
    if (!m_initialized) return false;

    bool allLoaded = true;
    for (u32 i = 0; i < static_cast<u32>(FontId::Count); ++i) {
        allLoaded &= m_fonts[i].create(fonts.get(static_cast<FontId>(i)));
    }
    return allLoaded;
}

void OpenGLRenderer::drawUI(const UIDrawList& list) {
    if (!m_initialized) return;
    m_ui.draw(list, m_shaders.get(ShaderId::UI), m_fonts, m_width, m_height);
}

void OpenGLRenderer::createRenderTargets(u32 width, u32 height) {
    destroyRenderTargets();

    if (m_msaaSamples <= 1 || width == 0 || height == 0) return;

    glGenRenderbuffers(1, &m_msaaColor);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaColor);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    glGenRenderbuffers(1, &m_msaaDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_msaaSamples, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_msaaFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaaColor);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_msaaDepth);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Fall back to drawing straight to the window
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[renderer] multisampled framebuffer incomplete (0x" << std::hex << status << std::dec << "); anti-aliasing off" << std::endl;
        destroyRenderTargets();
    }
}

void OpenGLRenderer::destroyRenderTargets() {
    if (m_msaaFramebuffer != 0) glDeleteFramebuffers(1, &m_msaaFramebuffer);
    if (m_msaaColor != 0) glDeleteRenderbuffers(1, &m_msaaColor);
    if (m_msaaDepth != 0) glDeleteRenderbuffers(1, &m_msaaDepth);

    m_msaaFramebuffer = 0;
    m_msaaColor = 0;
    m_msaaDepth = 0;
}

void OpenGLRenderer::beginFrame(){
    if (!m_initialized) return;
}

void OpenGLRenderer::beginMainPass(const ClearState& clearState){
    if (!m_initialized) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebuffer);

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

    OpenGLShader& background = m_shaders.get(ShaderId::Background);
    if (clearState.clearColor && background.bind()) {
        background.setVec3("u_TopColor", m_background.top);
        background.setVec3("u_BottomColor", m_background.bottom);
        background.setFloat("u_ViewportHeight", static_cast<f32>(m_height));

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glBindVertexArray(m_fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
}

void OpenGLRenderer::setLighting(const LightingState& lighting) {
    m_lighting = lighting;
}

void OpenGLRenderer::setBackground(const BackgroundGradient& background) {
    m_background = background;
}

BackgroundGradient OpenGLRenderer::getBackground() const {
    return m_background;
}

void OpenGLRenderer::setBackFaceTint(const Vec3& tint) {
    m_backFaceTint = tint;
}

Vec3 OpenGLRenderer::getBackFaceTint() const {
    return m_backFaceTint;
}

void OpenGLRenderer::draw(const DrawCommand& command) {
    if (!m_initialized || !command.mesh) return;

    // Faces: selected ones stay lit, just tinted
    if (command.showFaces) {
        OpenGLShader& lit = m_shaders.get(ShaderId::Lit);
        if (lit.bind()) {
            setLitUniforms(lit, command);

            lit.setVec3("u_Color", SELECTED_FACE_COLOR);
            for (const FaceHandle& face : command.highlightedFaces) {
                command.mesh->drawFace(face);
            }

            // Offset pushes the rest back so selected faces and edges win the depth test
            lit.setVec3("u_Color", FACE_COLOR);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            command.mesh->drawFaces();
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    OpenGLShader& shader = m_shaders.get(ShaderId::Unlit);
    if (!shader.bind()) return;

    shader.setMat4("u_MVP", command.mvp.m);
    shader.setInt("u_PointShape", POINT_SQUARE);
    shader.setFloat("u_DepthBias", 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Edges: selected ones get a wide faint glow under a crisp line
    if (command.showEdges) {
        if (!command.highlightedEdges.empty()) {
            // Two overlapping bands fake a soft falloff, since lines have no width-wise gradient
            glDepthMask(GL_FALSE);
            shader.setVec3("u_Color", SELECTED_COLOR);
            shader.setFloat("u_Alpha", SELECTED_GLOW_ALPHA * 0.5f);
            glLineWidth(SELECTED_EDGE_GLOW_WIDTH);
            for (const EdgeHandle& edge : command.highlightedEdges) command.mesh->drawEdge(edge);
            glLineWidth(SELECTED_EDGE_INNER_GLOW_WIDTH);
            for (const EdgeHandle& edge : command.highlightedEdges) command.mesh->drawEdge(edge);
            glDepthMask(GL_TRUE);

            shader.setFloat("u_Alpha", 1.0f);
            glLineWidth(SELECTED_EDGE_WIDTH);
            for (const EdgeHandle& edge : command.highlightedEdges) command.mesh->drawEdge(edge);
        }

        shader.setVec3("u_Color", EDGE_COLOR);
        shader.setFloat("u_Alpha", 1.0f);
        glLineWidth(EDGE_WIDTH);
        command.mesh->drawEdges();
    }

    // Vertices: round points; selected ones layer glow, dark outline, and purple center like a light marker
    if (command.showVerts) {
        glEnable(GL_POINT_SPRITE);
        shader.setFloat("u_DepthBias", VERTEX_DEPTH_BIAS);

        if (!command.highlightedVerts.empty()) {
            const auto drawSelected = [&](f32 size, i32 shape, const Vec3& color, f32 alpha) {
                glPointSize(size);
                shader.setInt("u_PointShape", shape);
                shader.setVec3("u_Color", color);
                shader.setFloat("u_Alpha", alpha);
                for (const VertexHandle& vertex : command.highlightedVerts) command.mesh->drawVertex(vertex);
            };

            // Only the center writes depth, so the layers under it don't block it
            glDepthMask(GL_FALSE);
            drawSelected(SELECTED_VERTEX_GLOW_SIZE, POINT_GLOW, SELECTED_COLOR, SELECTED_GLOW_ALPHA);
            drawSelected(SELECTED_VERTEX_OUTLINE_SIZE, POINT_DISC, OUTLINE_COLOR, 1.0f);
            glDepthMask(GL_TRUE);
            drawSelected(SELECTED_VERTEX_SIZE, POINT_DISC, SELECTED_COLOR, 1.0f);
        }

        glPointSize(VERTEX_SIZE);
        shader.setInt("u_PointShape", POINT_DISC);
        shader.setVec3("u_Color", VERTEX_COLOR);
        shader.setFloat("u_Alpha", 1.0f);
        command.mesh->drawVertices();

        shader.setInt("u_PointShape", POINT_SQUARE);
        shader.setFloat("u_DepthBias", 0.0f);
        glDisable(GL_POINT_SPRITE);
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void OpenGLRenderer::setLitUniforms(OpenGLShader& lit, const DrawCommand& command) {
    const Mat4 normalMatrix = Mat4::transpose(Mat4::inverse(command.model));

    lit.setMat4("u_MVP", command.mvp.m);
    lit.setMat4("u_Model", command.model.m);
    lit.setMat4("u_NormalMatrix", normalMatrix.m);
    lit.setVec3("u_BackFaceTint", m_backFaceTint);
    lit.setVec3("u_AmbientColor", m_lighting.ambientColor);
    lit.setFloat("u_AmbientStrength", m_lighting.ambientStrength);

    const u32 count = std::min(m_lighting.directionalCount, MAX_DIRECTIONAL_LIGHTS);
    lit.setInt("u_DirectionalLightCount", static_cast<i32>(count));
    if (count > 0) {
        lit.setVec3Array("u_DirectionalLightDirections[0]", m_lighting.directionalDirections, count);
        lit.setVec3Array("u_DirectionalLightColors[0]", m_lighting.directionalColors, count);
    }

    const u32 localCount = std::min(m_lighting.localCount, MAX_LOCAL_LIGHTS);
    lit.setInt("u_LocalLightCount", static_cast<i32>(localCount));
    if (localCount > 0) {
        lit.setVec3Array("u_LocalLightPositions[0]", m_lighting.localPositions, localCount);
        lit.setVec3Array("u_LocalLightDirections[0]", m_lighting.localDirections, localCount);
        lit.setVec3Array("u_LocalLightColors[0]", m_lighting.localColors, localCount);
        lit.setFloatArray("u_LocalLightRanges[0]", m_lighting.localRanges, localCount);
        lit.setFloatArray("u_LocalLightCosInner[0]", m_lighting.localCosInner, localCount);
        lit.setFloatArray("u_LocalLightCosOuter[0]", m_lighting.localCosOuter, localCount);
    }
}

void OpenGLRenderer::drawText3D(const DrawText3DCommand& command) {
    if (command.text.empty()) return;

    constexpr f32 glyphAspect = 16.0f / 24.0f;

    const f32 charHeight = command.size;
    const f32 charWidth = command.size * glyphAspect;

    std::vector<f32> vertices;

    // 6 vertices per character, 5 floats per vertex.
    vertices.reserve(command.text.size() * 6 * 5);

    f32 currentX = 0.0f;
    f32 currentY = 0.0f;

    for (char character : command.text) {
        if (character == '\n') {
            currentX = 0.0f;
            currentY -= charHeight;
            continue;
        }

        GlyphUV uv = FontAtlas::glyphUV(character);

        Vec3 origin = command.position + command.right * currentX + command.up * currentY;
        Vec3 topLeft = origin + command.up * charHeight;
        Vec3 bottomLeft = origin;
        Vec3 bottomRight = origin + command.right * charWidth;
        Vec3 topRight = origin + command.right * charWidth + command.up * charHeight;

        f32 characterVertices[] = {
            topLeft.x,
            topLeft.y,
            topLeft.z,
            uv.u0,
            uv.v0,

            bottomLeft.x,
            bottomLeft.y,
            bottomLeft.z,
            uv.u0,
            uv.v1,

            bottomRight.x,
            bottomRight.y,
            bottomRight.z,
            uv.u1,
            uv.v1,


            topLeft.x,
            topLeft.y,
            topLeft.z,
            uv.u0,
            uv.v0,

            bottomRight.x,
            bottomRight.y,
            bottomRight.z,
            uv.u1,
            uv.v1,

            topRight.x,
            topRight.y,
            topRight.z,
            uv.u1,
            uv.v0
        };

        vertices.insert(
            vertices.end(),
            std::begin(characterVertices),
            std::end(characterVertices)
        );

        currentX += charWidth;
    }

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_text3DVBO
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(f32),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    OpenGLShader& shader = m_shaders.get(ShaderId::WorldText);
    shader.bind();
    shader.setVec3("u_Color", {1.0f, 1.0f, 1.0f});
    shader.setMat4("u_MVP", command.mvp.m);
    shader.setInt("u_Texture", 0);

    glActiveTexture(GL_TEXTURE0);

    glBindTexture(
        GL_TEXTURE_2D,
        m_fonts[static_cast<u32>(FontId::Console)].getTexture()
    );

    glBindVertexArray(m_text3DVAO);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(
            vertices.size() / 5
        )
    );

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::drawGrid(const DrawGridCommand& command) {
    if (!m_initialized) return;

    constexpr f32 baseSpacing = 0.2f;
    constexpr f32 levelFactor = 5.0f;
    constexpr f32 baseDistance = 2.0f;

    // Pick the spacing level from zoom; the fractional part blends to the next level.
    f32 level = std::log(command.cameraDistance / baseDistance) / std::log(levelFactor);
    if (level < 0.0f) level = 0.0f;

    f32 levelIndex = std::floor(level);
    f32 spacing = baseSpacing * std::pow(levelFactor, levelIndex);

    OpenGLShader& shader = m_shaders.get(ShaderId::Grid);
    if (!shader.bind()) return;

    shader.setMat4("u_ViewProjection", command.viewProjection.m);
    shader.setMat4("u_InverseViewProjection", Mat4::inverse(command.viewProjection).m);
    shader.setVec3("u_CameraPosition", command.cameraPosition);
    shader.setFloat("u_FarPlane", command.farPlane);
    shader.setFloat("u_Spacing", spacing);
    shader.setFloat("u_LevelBlend", level - levelIndex);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(m_fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void OpenGLRenderer::drawImage(const DrawImageCommand& command) {
    if (!m_initialized || command.texture == 0) return;

    OpenGLShader& shader = m_shaders.get(ShaderId::Image);
    if (!shader.bind()) return;

    shader.setMat4("u_MVP", command.mvp.m);
    shader.setFloat("u_Opacity", command.opacity);
    shader.setInt("u_Texture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, command.texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!command.depthTest) glDisable(GL_DEPTH_TEST);
    // A see-through image doesn't hide what's drawn after it
    glDepthMask(command.depthTest && command.opacity >= 0.999f ? GL_TRUE : GL_FALSE);

    glBindVertexArray(m_fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}

u32 OpenGLRenderer::createTexture(const u8* pixels, u32 width, u32 height) {
    if (!m_initialized || !pixels || width == 0 || height == 0) return 0;

    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (width > static_cast<u32>(maxSize) || height > static_cast<u32>(maxSize)) return 0;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Mipmaps keep a big picture smooth when it's far away
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

void OpenGLRenderer::destroyTexture(u32 texture) {
    if (!m_initialized || texture == 0) return;
    const GLuint name = texture;
    glDeleteTextures(1, &name);
}

void OpenGLRenderer::drawDebugLine(
    const Vec3& start,
    const Vec3& end,
    const Mat4& mvp
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

    OpenGLShader& shader = m_shaders.get(ShaderId::Unlit);
    shader.bind();
    shader.setMat4("u_MVP", mvp.m);
    shader.setVec3("u_Color", {1.0f, 1.0f, 1.0f});
    shader.setFloat("u_Alpha", 1.0f);
    shader.setInt("u_PointShape", POINT_SQUARE);
    shader.setFloat("u_DepthBias", 0.0f);

    glDrawArrays(GL_LINES, 0, 2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OpenGLRenderer::endMainPass(){
    if (!m_initialized) return;
    if (m_msaaFramebuffer == 0) return;

    const GLint width = static_cast<GLint>(m_width);
    const GLint height = static_cast<GLint>(m_height);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::endFrame(){
    if (!m_initialized) return;
}

void OpenGLRenderer::resize(u32 width, u32 height){
    m_width = width;
    m_height = height;

    if (!m_initialized) return;

    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    createRenderTargets(width, height);
}

RendererBackend OpenGLRenderer::getBackend() const{
    return RendererBackend::RB_OpenGL;
}

const char* OpenGLRenderer::getBackendName() const{
    return "OpenGL";
}
