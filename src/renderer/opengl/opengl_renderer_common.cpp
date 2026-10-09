#include "renderer/opengl/opengl_renderer.hpp"
#include "core/input/contexts.hpp"
#include "core/font/font_atlas.hpp"

#include <iostream>
#include <algorithm>
#include <cmath>

#include <glad/glad.h>
#include "renderer/opengl/opengl_counters.hpp"

namespace {
    // Dracula-themed: a cool gray surface, edges in the theme's darker background
    const Vec3 EDGE_COLOR { 0.13f, 0.13f, 0.17f };
    const Vec3 VERTEX_COLOR { 0.10f, 0.10f, 0.13f };
    const Vec3 OUTLINE_COLOR { 0.06f, 0.06f, 0.08f };
    // Hard edges on a smooth-shaded mesh: Dracula cyan
    const Vec3 HARD_EDGE_COLOR { 0.55f, 0.91f, 0.99f };
    // UV seams: Dracula green
    const Vec3 SEAM_COLOR { 0.31f, 0.98f, 0.48f };

    // Selection matches the light markers and UI accent: Dracula purple with a soft glow
    const Vec3 SELECTED_COLOR { 0.74f, 0.58f, 0.98f };
    const Vec3 SELECTED_FACE_COLOR { 0.46f, 0.34f, 0.74f };
    // How far selected faces' base color goes toward SELECTED_FACE_COLOR; their shading stays
    constexpr f32 SELECTED_FACE_MIX = 0.8f;
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

    // The lit shader reads the lights from one buffer at a fixed binding, and its base color map from its own unit
    OpenGLShader& lit = m_shaders.get(ShaderId::Lit);
    lit.bindUniformBlock("Lighting", 0);
    if (lit.bind()) lit.setInt("u_BaseColorMap", MAP_TEXTURE_UNIT);

    // Maps repeat past 0..1 (pictures themselves clamp, for reference images); the sampler stays on the maps' unit
    glGenSamplers(1, &m_mapSampler);
    glSamplerParameteri(m_mapSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(m_mapSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(m_mapSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(m_mapSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindSampler(MAP_TEXTURE_UNIT, m_mapSampler);
    m_lightingDirty = true;

    m_ui.create();

    return true;
}

void OpenGLRenderer::destroyResources() {
    if (m_lightingBuffer != 0) glDeleteBuffers(1, &m_lightingBuffer);
    m_lightingBuffer = 0;
    if (m_mapSampler != 0) glDeleteSamplers(1, &m_mapSampler);
    m_mapSampler = 0;
    if (m_timerQueries[0] != 0) glDeleteQueries(TIMER_QUERIES, m_timerQueries);
    for (u32 i = 0; i < TIMER_QUERIES; ++i) {
        m_timerQueries[i] = 0;
        m_timerPending[i] = false;
    }
    m_shaders.destroy();
    destroyPreviewResources();
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

void OpenGLRenderer::setSceneViewport(u32 x, u32 y, u32 width, u32 height) {
    if (!m_initialized) return;
    // GL counts rows from the bottom
    const i32 bottom = static_cast<i32>(m_height) - static_cast<i32>(y + height);
    glViewport(static_cast<GLint>(x), bottom, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void OpenGLRenderer::drawUI(const UIDrawList& list) {
    if (!m_initialized) return;
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
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

    glCounters() = {};
    if (m_timerQueries[0] == 0) glGenQueries(TIMER_QUERIES, m_timerQueries);

    // Collect the slot about to be reused if its result is in; otherwise skip timing this frame
    const u32 slot = m_timerFrame % TIMER_QUERIES;
    if (m_timerPending[slot]) {
        GLint available = 0;
        glGetQueryObjectiv(m_timerQueries[slot], GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) return;

        GLuint64 nanoseconds = 0;
        glGetQueryObjectui64v(m_timerQueries[slot], GL_QUERY_RESULT, &nanoseconds);
        m_gpuMilliseconds = static_cast<f32>(nanoseconds) / 1.0e6f;
        m_timerPending[slot] = false;
    }

    glBeginQuery(GL_TIME_ELAPSED, m_timerQueries[slot]);
    m_timerPending[slot] = true;
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
        drawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
}

void OpenGLRenderer::setLighting(const LightingState& lighting) {
    m_lighting = lighting;
    m_lightingDirty = true;
}

void OpenGLRenderer::setExposure(f32 stops) {
    if (stops != m_exposure) m_lightingDirty = true;
    m_exposure = stops;
}

void OpenGLRenderer::setBackground(const BackgroundGradient& background) {
    m_background = background;
}

BackgroundGradient OpenGLRenderer::getBackground() const {
    return m_background;
}

void OpenGLRenderer::setBackFaceTint(const Vec3& tint) {
    m_backFaceTint = tint;
    m_lightingDirty = true;
}

Vec3 OpenGLRenderer::getBackFaceTint() const {
    return m_backFaceTint;
}

namespace {
    // The Lighting block in lit.frag, std140: every member a vec4 (or ivec4), so no padding rules to trip on
    struct LightingBlock {
        f32 directionalDirections[MAX_DIRECTIONAL_LIGHTS][4];
        f32 directionalColors[MAX_DIRECTIONAL_LIGHTS][4];
        f32 localPositions[MAX_LOCAL_LIGHTS][4];
        f32 localDirections[MAX_LOCAL_LIGHTS][4];
        f32 localColors[MAX_LOCAL_LIGHTS][4];
        f32 skyColor[4];
        f32 groundColor[4];
        f32 cameraPosition[4];
        f32 backFaceTint[4];
        i32 lightCounts[4];
    };
    static_assert(sizeof(LightingBlock) == 37 * 16, "std140 layout of the Lighting block");

    void put(f32 (&out)[4], const Vec3& value, f32 w = 0.0f) {
        out[0] = value.x;
        out[1] = value.y;
        out[2] = value.z;
        out[3] = w;
    }

    constexpr u32 LIGHTING_BINDING = 0;
}

void OpenGLRenderer::uploadLighting() {
    if (!m_lightingDirty) return;

    LightingBlock block {};
    const u32 directional = std::min(m_lighting.directionalCount, MAX_DIRECTIONAL_LIGHTS);
    for (u32 i = 0; i < directional; ++i) {
        put(block.directionalDirections[i], m_lighting.directionalDirections[i]);
        put(block.directionalColors[i], m_lighting.directionalColors[i]);
    }

    const u32 local = std::min(m_lighting.localCount, MAX_LOCAL_LIGHTS);
    for (u32 i = 0; i < local; ++i) {
        put(block.localPositions[i], m_lighting.localPositions[i], m_lighting.localRanges[i]);
        put(block.localDirections[i], m_lighting.localDirections[i], m_lighting.localCosInner[i]);
        put(block.localColors[i], m_lighting.localColors[i], m_lighting.localCosOuter[i]);
    }

    put(block.skyColor, m_lighting.skyColor, std::exp2(m_exposure));
    put(block.groundColor, m_lighting.groundColor);
    put(block.cameraPosition, m_lighting.cameraPosition, m_lighting.uvChecker ? 1.0f : 0.0f);
    put(block.backFaceTint, m_backFaceTint);
    block.lightCounts[0] = static_cast<i32>(directional);
    block.lightCounts[1] = static_cast<i32>(local);

    if (m_lightingBuffer == 0) glGenBuffers(1, &m_lightingBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_lightingBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(block), &block, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, LIGHTING_BINDING, m_lightingBuffer);

    m_lightingDirty = false;
}

void OpenGLRenderer::setSurfaceUniforms(OpenGLShader& lit, const SurfaceLook& surface) {
    // Bound every time: the maps' unit is only ever used by this, but a cheap rebind keeps that from mattering
    glActiveTexture(GL_TEXTURE0 + MAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D, surface.baseColorMap);
    glActiveTexture(GL_TEXTURE0);

    // A run of draws in the same material (most objects, sorted by material) sends it once
    const bool same = m_lastSurfaceValid && sameSurface(m_lastSurface, surface);
    if (same) return;

    lit.setVec3("u_BaseColor", surface.baseColor);
    lit.setFloat("u_Roughness", surface.roughness);
    lit.setFloat("u_Metallic", surface.metallic);
    lit.setVec3("u_EmissiveColor", surface.emissiveColor);
    lit.setFloat("u_EmissiveStrength", surface.emissiveStrength);
    lit.setFloat("u_Opacity", surface.blend || surface.alphaCutoff >= 0.0f ? surface.opacity : 1.0f);
    lit.setFloat("u_AlphaCutoff", surface.alphaCutoff);
    lit.setInt("u_HasBaseColorMap", surface.baseColorMap != 0 ? 1 : 0);
    lit.setInt("u_BackFaces", surface.backFaces == BackFaces::Tinted ? 0 : 1);

    m_lastSurface = surface;
    m_lastSurfaceValid = true;
}

void OpenGLRenderer::drawSurfaceRange(OpenGLShader& lit, IMesh& mesh, const SurfaceLook& surface, u32 firstIndex, u32 indexCount, bool whole) {
    setSurfaceUniforms(lit, surface);

    if (surface.blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }

    // One pass per side to draw: a see-through surface shows its far side first, then its near side over it
    GLenum culls[2] = { GL_NONE, GL_NONE };
    u32 passes = 1;
    if (surface.backFaces == BackFaces::Culled) culls[0] = GL_BACK;
    else if (surface.blend) {
        culls[0] = GL_FRONT;
        culls[1] = GL_BACK;
        passes = 2;
    }

    // Offset pushes faces back so selected faces and edges win the depth test
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    for (u32 pass = 0; pass < passes; ++pass) {
        if (culls[pass] == GL_NONE) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(culls[pass]);
        }

        if (whole) mesh.drawFaces();
        else mesh.drawFaceRange(firstIndex, indexCount);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_CULL_FACE);

    if (surface.blend) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void OpenGLRenderer::draw(const DrawCommand& command) {
    if (!m_initialized || !command.mesh) return;
    IMesh& mesh = *command.mesh;

    // Faces: each part in its material; selected faces stay lit, just tinted
    const bool anyFaces = command.showFaces || !command.highlightedFaces.empty();
    OpenGLShader& lit = m_shaders.get(ShaderId::Lit);
    if (anyFaces && lit.bind()) {
        setLitUniforms(lit, command);

        if (!command.highlightedFaces.empty()) {
            setSurfaceUniforms(lit, command.surface);
            lit.setFloat("u_Highlight", SELECTED_FACE_MIX);
            mesh.drawFaceSet(command.highlightedFaces);
            lit.setFloat("u_Highlight", 0.0f);
        }

        if (command.showFaces) {
            if (command.parts.empty()) {
                drawSurfaceRange(lit, mesh, command.surface, 0, 0, true);
            } else {
                for (const DrawPart& part : command.parts) drawSurfaceRange(lit, mesh, part.surface, part.firstIndex, part.indexCount, false);
            }
        }
    }

    if (!command.showEdges && !command.showVerts) return;

    OpenGLShader& shader = m_shaders.get(ShaderId::Unlit);
    if (!shader.bind()) return;

    shader.setMat4("u_MVP", command.mvp.m);
    shader.setInt("u_PointShape", POINT_SQUARE);
    shader.setFloat("u_DepthBias", 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Edges: selected ones get a wide faint glow under a crisp line; an outlined object is every edge, in one draw
    if (command.showEdges) {
        const bool highlighted = command.outlineAll || !command.highlightedEdges.empty();
        const auto drawHighlighted = [&] {
            if (command.outlineAll) mesh.drawEdges();
            else mesh.drawEdgeSet(command.highlightedEdges);
        };

        if (highlighted) {
            // Two overlapping bands fake a soft falloff, since lines have no width-wise gradient
            glDepthMask(GL_FALSE);
            shader.setVec3("u_Color", SELECTED_COLOR);
            shader.setFloat("u_Alpha", SELECTED_GLOW_ALPHA * 0.5f);
            glLineWidth(SELECTED_EDGE_GLOW_WIDTH);
            drawHighlighted();
            glLineWidth(SELECTED_EDGE_INNER_GLOW_WIDTH);
            drawHighlighted();
            glDepthMask(GL_TRUE);

            shader.setFloat("u_Alpha", 1.0f);
            glLineWidth(SELECTED_EDGE_WIDTH);
            drawHighlighted();
        }

        // Hard edges after the selection, so a selected one keeps the selection color, and before the rest
        if (!command.outlineAll && !command.hardEdges.empty()) {
            shader.setVec3("u_Color", HARD_EDGE_COLOR);
            shader.setFloat("u_Alpha", 1.0f);
            glLineWidth(SELECTED_EDGE_WIDTH);
            mesh.drawHardEdgeSet(command.hardEdges);
        }
        if (!command.outlineAll && !command.seamEdges.empty()) {
            shader.setVec3("u_Color", SEAM_COLOR);
            shader.setFloat("u_Alpha", 1.0f);
            glLineWidth(SELECTED_EDGE_WIDTH);
            mesh.drawSeamEdgeSet(command.seamEdges);
        }

        // An outlined object's own lines are all drawn already, in the selection color
        if (!command.outlineAll) {
            shader.setVec3("u_Color", EDGE_COLOR);
            shader.setFloat("u_Alpha", 1.0f);
            glLineWidth(EDGE_WIDTH);
            mesh.drawEdges();
        }
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
                mesh.drawVertexSet(command.highlightedVerts);
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
        mesh.drawVertices();

        shader.setInt("u_PointShape", POINT_SQUARE);
        shader.setFloat("u_DepthBias", 0.0f);
        glDisable(GL_POINT_SPRITE);
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void OpenGLRenderer::setLitUniforms(OpenGLShader& lit, const DrawCommand& command) {
    uploadLighting();

    // Per object: only its matrices; the lights are in the Lighting buffer, the material set per part
    const Mat4 normalMatrix = Mat4::transpose(Mat4::inverse(command.model));
    lit.setMat4("u_MVP", command.mvp.m);
    lit.setMat4("u_Model", command.model.m);
    lit.setMat4("u_NormalMatrix", normalMatrix.m);

    if (!m_litConstantsSet) {
        lit.setVec3("u_HighlightColor", SELECTED_FACE_COLOR);
        lit.setFloat("u_Highlight", 0.0f);
        m_litConstantsSet = true;
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

    drawArrays(
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
    drawArrays(GL_TRIANGLES, 0, 3);
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
    drawArrays(GL_TRIANGLES, 0, 6);
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

    drawArrays(GL_LINES, 0, 2);

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

    // beginFrame skips timing when the slot's last result hadn't come back yet
    GLint active = 0;
    glGetQueryiv(GL_TIME_ELAPSED, GL_CURRENT_QUERY, &active);
    if (active != 0) {
        glEndQuery(GL_TIME_ELAPSED);
        ++m_timerFrame;
    }

    m_lastStats = glCounters();
    m_lastStats.gpuMilliseconds = m_gpuMilliseconds;
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
