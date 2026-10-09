#include "renderer/opengl/opengl_renderer.hpp"

#include <cmath>
#include <vector>

#include <glad/glad.h>
#include "renderer/opengl/opengl_counters.hpp"

// Material swatches: a smooth sphere in the material, lit by a fixed studio setup so swatches look the same whatever
// the scene's lights, rendered with the viewport's own face shader into a texture the UI draws

namespace {
    constexpr u32 SPHERE_SEGMENTS = 64;
    constexpr u32 SPHERE_RINGS = 32;
    constexpr u32 CHECKER_CELLS = 8;
    constexpr f32 PREVIEW_FOV = 0.55f;
    constexpr f32 PREVIEW_DISTANCE = 4.0f;
}

void OpenGLRenderer::createPreviewSphere() {
    // Position, normal, and UV per vertex; on a unit sphere the normal is the position
    std::vector<f32> vertices;
    for (u32 ring = 0; ring <= SPHERE_RINGS; ++ring) {
        const f32 polar = 3.14159265f * static_cast<f32>(ring) / SPHERE_RINGS;
        for (u32 segment = 0; segment <= SPHERE_SEGMENTS; ++segment) {
            const f32 azimuth = 6.2831853f * static_cast<f32>(segment) / SPHERE_SEGMENTS;
            const f32 x = std::sin(polar) * std::cos(azimuth);
            const f32 y = std::cos(polar);
            const f32 z = -std::sin(polar) * std::sin(azimuth);
            const f32 u = static_cast<f32>(segment) / SPHERE_SEGMENTS;
            const f32 v = static_cast<f32>(ring) / SPHERE_RINGS;
            vertices.insert(vertices.end(), { x, y, z, x, y, z, u, v });
        }
    }

    // Counterclockwise seen from outside
    std::vector<u32> indices;
    const u32 stride = SPHERE_SEGMENTS + 1;
    for (u32 ring = 0; ring < SPHERE_RINGS; ++ring) {
        for (u32 segment = 0; segment < SPHERE_SEGMENTS; ++segment) {
            const u32 a = ring * stride + segment;
            const u32 b = a + stride;
            indices.insert(indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
        }
    }
    m_previewSphereIndices = static_cast<u32>(indices.size());

    glGenVertexArrays(1, &m_previewSphereVAO);
    glGenBuffers(1, &m_previewSphereVBO);
    glGenBuffers(1, &m_previewSphereEBO);

    glBindVertexArray(m_previewSphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_previewSphereVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(f32)), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_previewSphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(u32)), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(3 * sizeof(f32)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(6 * sizeof(f32)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // A light and dark checkerboard behind see-through materials
    std::vector<u8> checker(CHECKER_CELLS * CHECKER_CELLS * 4);
    for (u32 y = 0; y < CHECKER_CELLS; ++y) {
        for (u32 x = 0; x < CHECKER_CELLS; ++x) {
            const u8 shade = ((x + y) % 2 == 0) ? 120 : 70;
            u8* texel = &checker[(y * CHECKER_CELLS + x) * 4];
            texel[0] = texel[1] = texel[2] = shade;
            texel[3] = 255;
        }
    }
    glGenTextures(1, &m_checkerTexture);
    glBindTexture(GL_TEXTURE_2D, m_checkerTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CHECKER_CELLS, CHECKER_CELLS, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::destroyPreviewResources() {
    if (m_previewSphereVAO != 0) glDeleteVertexArrays(1, &m_previewSphereVAO);
    if (m_previewSphereVBO != 0) glDeleteBuffers(1, &m_previewSphereVBO);
    if (m_previewSphereEBO != 0) glDeleteBuffers(1, &m_previewSphereEBO);
    if (m_checkerTexture != 0) glDeleteTextures(1, &m_checkerTexture);
    if (m_previewFramebuffer != 0) glDeleteFramebuffers(1, &m_previewFramebuffer);
    if (m_previewColor != 0) glDeleteRenderbuffers(1, &m_previewColor);
    if (m_previewDepth != 0) glDeleteRenderbuffers(1, &m_previewDepth);
    if (m_previewResolve != 0) glDeleteFramebuffers(1, &m_previewResolve);

    m_previewSphereVAO = m_previewSphereVBO = m_previewSphereEBO = m_checkerTexture = 0;
    m_previewFramebuffer = m_previewColor = m_previewDepth = m_previewResolve = 0;
    m_previewSize = 0;
}

// The multisampled target the sphere is drawn into, remade when the size changes
bool OpenGLRenderer::preparePreviewTarget(u32 size) {
    if (m_previewFramebuffer != 0 && m_previewSize == size) return true;

    if (m_previewFramebuffer != 0) glDeleteFramebuffers(1, &m_previewFramebuffer);
    if (m_previewColor != 0) glDeleteRenderbuffers(1, &m_previewColor);
    if (m_previewDepth != 0) glDeleteRenderbuffers(1, &m_previewDepth);
    if (m_previewResolve == 0) glGenFramebuffers(1, &m_previewResolve);

    const GLsizei samples = std::max<GLsizei>(m_msaaSamples, 1);
    glGenRenderbuffers(1, &m_previewColor);
    glBindRenderbuffer(GL_RENDERBUFFER, m_previewColor);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
    glGenRenderbuffers(1, &m_previewDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_previewDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_previewFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_previewFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_previewColor);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_previewDepth);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_previewSize = complete ? size : 0;
    return complete;
}

u32 OpenGLRenderer::renderMaterialPreview(const SurfaceLook& look, u32 size, u32 texture) {
    if (!m_initialized || size == 0) return texture;
    if (m_previewSphereVAO == 0) createPreviewSphere();
    if (!preparePreviewTarget(size)) return texture;

    if (texture == 0) {
        GLuint created = 0;
        glGenTextures(1, &created);
        glBindTexture(GL_TEXTURE_2D, created);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(size), static_cast<GLsizei>(size), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        texture = created;
    }

    // Studio lighting in place of the scene's, and no exposure, so every swatch is lit the same way
    const LightingState sceneLighting = m_lighting;
    const f32 sceneExposure = m_exposure;
    LightingState studio;
    studio.directionalDirections[0] = Vec3(0.55f, -0.6f, -0.6f).normalized();
    studio.directionalColors[0] = Vec3(1.6f, 1.55f, 1.5f);
    studio.directionalDirections[1] = Vec3(-0.8f, 0.1f, -0.4f).normalized();
    studio.directionalColors[1] = Vec3(0.35f, 0.38f, 0.45f);
    studio.directionalCount = 2;
    studio.skyColor = Vec3(0.42f, 0.44f, 0.5f);
    studio.groundColor = Vec3(0.12f, 0.12f, 0.14f);
    studio.cameraPosition = Vec3(0.0f, 0.0f, PREVIEW_DISTANCE);
    m_lighting = studio;
    m_exposure = 0.0f;
    m_lightingDirty = true;

    glBindFramebuffer(GL_FRAMEBUFFER, m_previewFramebuffer);
    glViewport(0, 0, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // See-through materials sit on a checkerboard
    if (look.blend) {
        DrawImageCommand checker;
        checker.texture = m_checkerTexture;
        checker.mvp = Mat4::scale(Vec3(2.0f, 2.0f, 1.0f));
        checker.depthTest = false;
        drawImage(checker);
    }

    // Uses draw's face pass, with a sphere instead of an object mesh
    OpenGLShader& lit = m_shaders.get(ShaderId::Lit);
    if (lit.bind() && look.opacity > 0.0f) {
        DrawCommand command;
        command.surface = look;
        command.model = Mat4();
        // Drawn upside down, so the texture's first row is the swatch's top, as the UI expects of every texture;
        // that turns the winding around too
        command.mvp = Mat4::scale(Vec3(1.0f, -1.0f, 1.0f)) * Mat4::perspective(PREVIEW_FOV, 1.0f, 0.1f, 10.0f)
                    * Mat4::lookAt(Vec3(0.0f, 0.0f, PREVIEW_DISTANCE), Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f));
        setLitUniforms(lit, command);
        setSurfaceUniforms(lit, look);
        lit.setFloat("u_Highlight", 0.0f);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CW);
        if (look.blend) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }

        // A see-through sphere shows its far side through the near one when the material draws back faces
        glBindVertexArray(m_previewSphereVAO);
        if (look.blend && look.backFaces != BackFaces::Culled) {
            glCullFace(GL_FRONT);
            drawElements(GL_TRIANGLES, static_cast<GLsizei>(m_previewSphereIndices), GL_UNSIGNED_INT, nullptr);
        }
        glCullFace(GL_BACK);
        drawElements(GL_TRIANGLES, static_cast<GLsizei>(m_previewSphereIndices), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glFrontFace(GL_CCW);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    // Resolve the samples into the texture, then give it mipmaps for the small swatches in lists
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_previewFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_previewResolve);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBlitFramebuffer(0, 0, static_cast<GLint>(size), static_cast<GLint>(size), 0, 0, static_cast<GLint>(size), static_cast<GLint>(size),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_2D, texture);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_lighting = sceneLighting;
    m_exposure = sceneExposure;
    m_lightingDirty = true;
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
    return texture;
}
