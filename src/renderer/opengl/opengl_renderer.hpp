#pragma once

#include <memory>
#include "renderer/renderer.hpp"
#include "renderer/opengl/opengl_shader.hpp"
#include "renderer/opengl/opengl_mesh.hpp"

class OpenGLRenderer : public IRenderer {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override;

    bool initialize(void* window, void* surface, const RendererConfig& config) override;
    void shutdown() override;

    void present() override;
    void setVSync(bool enabled) override;
    
    void beginFrame() override;
    void beginMainPass(const ClearState& clearState) override;
    void draw(const DrawCommand& command) override;
    void endMainPass() override;
    void endFrame() override;
    void resize(u32 width, u32 height) override;

    RendererBackend getBackend() const override;
    const char* getBackendName() const override;

private:
    void* m_window = nullptr;
    void* m_surface = nullptr;
    void* m_glContext = nullptr;

    u32 m_width = 0;
    u32 m_height = 0;
    bool m_vsyncEnabled = true;

    std::unique_ptr<OpenGLShader> m_testShader;
    
    bool m_initialized = false;
};