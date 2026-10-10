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
    void setSceneViewport(u32 x, u32 y, u32 width, u32 height) override;
    void setLighting(const LightingState& lighting) override;
    void setBackground(const BackgroundGradient& background) override;
    BackgroundGradient getBackground() const override;
    void setExposure(f32 stops) override;
    f32 getExposure() const override { return m_exposure; }
    void setBackFaceTint(const Vec3& tint) override;
    Vec3 getBackFaceTint() const override;
    void draw(const DrawCommand& command) override;
    void drawText3D(const DrawText3DCommand& command) override;
    void drawGrid(const DrawGridCommand& command) override;
    void drawImage(const DrawImageCommand& command) override;
    u32 createTexture(const u8* pixels, u32 width, u32 height) override;
    void destroyTexture(u32 texture) override;
    void updateTexture(u32 texture, u32 x, u32 y, u32 width, u32 height, const u8* pixels) override;
    void refreshTextureMipmaps(u32 texture) override;
    u32 renderMaterialPreview(const SurfaceLook& look, u32 size, u32 texture) override;
    void drawDebugLine(const Vec3& start, const Vec3& end, const Mat4& mvp) override;
    void drawUI(const UIDrawList& list) override;
    void endMainPass() override;
    void endFrame() override;
    void resize(u32 width, u32 height) override;

    RenderStats getStats() const override { return m_lastStats; }

    RendererBackend getBackend() const override;
    const char* getBackendName() const override;

private:
    bool createResources();
    void destroyResources();
    void setLitUniforms(OpenGLShader& lit, const DrawCommand& command);
    // Sends the lighting buffer if anything in it changed
    void uploadLighting();
    void setSurfaceUniforms(OpenGLShader& lit, const SurfaceLook& surface);
    // One material's faces (or every face when whole), with its blending and culling
    void drawSurfaceRange(OpenGLShader& lit, IMesh& mesh, const SurfaceLook& surface, u32 firstIndex, u32 indexCount, bool whole);

    void createPreviewSphere();
    bool preparePreviewTarget(u32 size);
    void destroyPreviewResources();

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
    f32 m_exposure = 0.0f;

    // The Lighting uniform buffer, and the material last sent to the lit shader (to skip sending it again)
    u32 m_lightingBuffer = 0;
    bool m_lightingDirty = true;
    SurfaceLook m_lastSurface;
    bool m_lastSurfaceValid = false;
    bool m_litConstantsSet = false;
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
    
    // Material previews (opengl_material_preview.cpp)
    u32 m_previewSphereVAO = 0;
    u32 m_previewSphereVBO = 0;
    u32 m_previewSphereEBO = 0;
    u32 m_previewSphereIndices = 0;
    u32 m_checkerTexture = 0;
    // Repeats base color maps; bound to MAP_TEXTURE_UNIT for good
    u32 m_mapSampler = 0;
    static constexpr u32 MAP_TEXTURE_UNIT = 1;
    u32 m_previewFramebuffer = 0;
    u32 m_previewColor = 0;
    u32 m_previewDepth = 0;
    u32 m_previewResolve = 0;
    u32 m_previewSize = 0;

    // GPU frame timing: queries in a ring, read back once their results are ready, so reading never stalls
    static constexpr u32 TIMER_QUERIES = 4;
    u32 m_timerQueries[TIMER_QUERIES] = {};
    bool m_timerPending[TIMER_QUERIES] = {};
    u32 m_timerFrame = 0;
    f32 m_gpuMilliseconds = -1.0f;
    RenderStats m_lastStats;

    bool m_initialized = false;
};