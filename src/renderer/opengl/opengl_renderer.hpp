#pragma once

#include <types>
#include <memory>
#include "renderer/renderer.hpp"
#include "renderer/opengl/shaders/opengl_shader_library.hpp"
#include "renderer/opengl/opengl_mesh.hpp"
#include "renderer/opengl/opengl_font.hpp"
#include "renderer/opengl/opengl_ui_renderer.hpp"

#include <array>

class OpenGLRenderer : public IRenderer {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override;

    bool initialize(void* window, void* surface, const RendererConfig& config) override;
    bool loadFonts(const FontLibrary& fonts) override;
    void shutdown() override;

    void present() override;
    void setVSync(bool enabled) override;
    bool getVSync() const override { return m_vsyncEnabled; }
    
    void beginFrame() override;
    void beginMainPass(const ClearState& clearState) override;
    void setLighting(const LightingState& lighting) override;
    void setBackground(const BackgroundGradient& background) override;
    BackgroundGradient getBackground() const override;
    void setBackFaceTint(const Vec3& tint) override;
    Vec3 getBackFaceTint() const override;
    void draw(const DrawCommand& command) override;
    void drawText3D(const DrawText3DCommand& command) override;
    void drawGrid(const DrawGridCommand& command) override;
    void drawDebugLine(const Vec3& start, const Vec3& end, const Mat4& mvp) override;
    void drawUI(const UIDrawList& list) override;
    void endMainPass() override;
    void endFrame() override;
    void resize(u32 width, u32 height) override;

    RendererBackend getBackend() const override;
    const char* getBackendName() const override;

private:
    bool createResources();
    void destroyResources();
    void setLitUniforms(OpenGLShader& lit, const DrawCommand& command);

    void createRenderTargets(u32 width, u32 height);
    void destroyRenderTargets();

    void* m_window = nullptr;
    void* m_surface = nullptr;
    void* m_glContext = nullptr;

    u32 m_width = 0;
    u32 m_height = 0;
    bool m_vsyncEnabled = true;

    OpenGLShaderLibrary m_shaders;
    LightingState m_lighting;
    Vec3 m_backFaceTint { 0.95f, 0.45f, 0.70f };
    BackgroundGradient m_background;

    std::array<OpenGLFont, static_cast<u32>(FontId::Count)> m_fonts;
    OpenGLUIRenderer m_ui;


    u32 m_text3DVAO = 0;
    u32 m_text3DVBO = 0;

    u32 m_debugLineVAO = 0;
    u32 m_debugLineVBO = 0;

    u32 m_fullscreenVAO = 0;

    // Scene is drawn into a multisampled framebuffer and resolved to the window in endMainPass
    u32 m_msaaFramebuffer = 0;
    u32 m_msaaColor = 0;
    u32 m_msaaDepth = 0;
    i32 m_msaaSamples = 4;
    
    bool m_initialized = false;
};