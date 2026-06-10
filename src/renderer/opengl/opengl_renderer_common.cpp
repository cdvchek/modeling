#include "renderer/opengl/opengl_renderer.hpp"

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
    if (!m_initialized) return;
    if (!command.mesh) return;

    m_testShader->bind();
    m_testShader->setMat4("u_MVP", command.mvp.m);

    // Solid faces
    m_testShader->setVec3("u_Color", Vec3(0.7f, 0.7f, 0.7f));
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    command.mesh->draw();

    // Black edges / wireframe
    m_testShader->setVec3("u_Color", Vec3(0.0f, 0.0f, 0.0f));
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    command.mesh->draw();

    // Black vertices / points
    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    glPointSize(8.0f);
    command.mesh->draw();

    // Restore default
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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
