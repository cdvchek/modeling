#include "renderer/opengl/opengl_renderer.hpp"
#include "core/input/contexts.hpp"

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
    if (!m_initialized || !command.mesh) {
        return;
    }

    u32 numHighlightedVerts = command.numHighlightedVerts;
    u32 numHighlightedEdges = command.numHighlightedEdges;
    u32 numHighlightedFaces = command.numHighlightedFaces;

    m_testShader->bind();
    m_testShader->setMat4("u_MVP", command.mvp.m);

    // Faces
    if (command.showFaces) {
        m_testShader->setVec3(
            "u_Color",
            Vec3(1.0f, 1.0f, 0.0f)
        );
    
        u32 faceOffset = numHighlightedVerts + numHighlightedEdges;
        for (u32 i = 0; i < numHighlightedFaces; ++i) {
            u32 faceIndex = command.selected[i + faceOffset];
            command.mesh->drawFace(faceIndex);
        }
    
        m_testShader->setVec3(
            "u_Color",
            Vec3(0.7f, 0.7f, 0.7f)
        );
    
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
    
        command.mesh->drawFaces();
    
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Edges
    if (command.showEdges) {
        m_testShader->setVec3(
            "u_Color",
            Vec3(1.0f, 1.0f, 0.0f)
        );
    
        u32 edgeOffset = numHighlightedVerts;
        for (u32 i = 0; i < numHighlightedEdges; ++i) {
            u32 edgeIndex = command.selected[i + edgeOffset];
            command.mesh->drawEdge(edgeIndex);
        }
    
        m_testShader->setVec3(
            "u_Color",
            Vec3(0.0f, 0.0f, 0.0f)
        );
    
        glLineWidth(2.0f);
        command.mesh->drawEdges();
    }

    // Vertices
    if (command.showVerts) {
        m_testShader->setVec3(
            "u_Color",
            Vec3(1.0f, 1.0f, 0.0f)
        );
    
        for (u32 i = 0; i < numHighlightedVerts; ++i) {
            u32 vertIndex = command.selected[i];
            command.mesh->drawVertex(vertIndex);
        }
    
        m_testShader->setVec3(
            "u_Color",
            Vec3(0.0f, 0.0f, 0.0f)
        );
    
        glPointSize(8.0f);
        command.mesh->drawVertices();
    }

    glBindVertexArray(0);
}

void OpenGLRenderer::drawPoint(const PointDrawCommand& command) {
    if (!m_initialized) return;

    m_testShader->bind();

    Mat4 model = Mat4::translation(command.position);
    Mat4 mvp = command.viewProjection * model;

    m_testShader->setMat4("u_MVP", mvp.m);
    m_testShader->setVec3("u_Color", command.color);

    glBindVertexArray(m_pointVAO);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPointSize(command.size);
    glDrawArrays(GL_POINTS, 0, 1);

    glBindVertexArray(0);
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
