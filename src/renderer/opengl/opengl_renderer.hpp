#pragma once

#include <types>
#include <memory>
#include "renderer/renderer.hpp"
#include "renderer/opengl/opengl_shader.hpp"
#include "renderer/opengl/opengl_mesh.hpp"
#include "renderer/opengl/opengl_font.hpp"

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
    void drawText(const std::string& text, f32 x, f32 y) override;
    void drawDebugLine(const Vec3& start, const Vec3& end) override;
    void drawConsoleBackground() override;
    void endMainPass() override;
    void endFrame() override;
    void resize(u32 width, u32 height) override;

    RendererBackend getBackend() const override;
    const char* getBackendName() const override;

private:
    virtual void drawCharacter(const OpenGLFont& font, char character, f32 x, f32 y);

    void* m_window = nullptr;
    void* m_surface = nullptr;
    void* m_glContext = nullptr;

    u32 m_width = 0;
    u32 m_height = 0;
    bool m_vsyncEnabled = true;

    std::unique_ptr<OpenGLShader> m_testShader;
    std::unique_ptr<OpenGLShader> m_consoleShader;
    std::unique_ptr<OpenGLShader> m_textShader;

    OpenGLFont m_consoleFont;

    u32 m_textVAO = 0;
    u32 m_textVBO = 0;

    u32 m_debugLineVAO = 0;
    u32 m_debugLineVBO = 0;
    
    bool m_initialized = false;
};