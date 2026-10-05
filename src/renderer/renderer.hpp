#pragma once

#include <types>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"
#include "renderer/gpu_mesh.hpp"
#include "core/font/font_library.hpp"
#include "ui/ui_draw_list.hpp"

struct WindowHandle;

enum class RendererBackend {
    RB_None,
    RB_OpenGL,
    RB_Vulkan,
    RB_DirectX12,
    RB_Metal
};

struct RendererConfig {
    u32 width = 1280;
    u32 height = 720;
    bool enableVSync = true;
    bool enableValidation = true;
};

struct ClearState {
    float r = 0.15f;
    float g = 0.15f;
    float b = 0.15f;
    float a = 1.0f;
    float depth = 1.0f;
    float stencil = 1.0f;
    bool clearColor = true;
    bool clearDepth = true;
    bool clearStencil = false;
};

constexpr u32 MAX_DIRECTIONAL_LIGHTS = 4;
constexpr u32 MAX_LOCAL_LIGHTS = 8;

struct BackgroundGradient {
    Vec3 top { 0.24f, 0.24f, 0.26f };
    Vec3 bottom { 0.11f, 0.11f, 0.12f };
};

struct LightingState {
    Vec3 ambientColor { 1.0f, 1.0f, 1.0f };
    f32 ambientStrength = 1.0f;

    // Directions are normalized and point the way the light travels; colors include intensity
    Vec3 directionalDirections[MAX_DIRECTIONAL_LIGHTS];
    Vec3 directionalColors[MAX_DIRECTIONAL_LIGHTS];
    u32 directionalCount = 0;

    // Point and spot lights; point lights use cosInner = -1, cosOuter = -2 so the cone never cuts them off
    Vec3 localPositions[MAX_LOCAL_LIGHTS];
    Vec3 localDirections[MAX_LOCAL_LIGHTS];
    Vec3 localColors[MAX_LOCAL_LIGHTS];
    f32 localRanges[MAX_LOCAL_LIGHTS] = {};
    f32 localCosInner[MAX_LOCAL_LIGHTS] = {};
    f32 localCosOuter[MAX_LOCAL_LIGHTS] = {};
    u32 localCount = 0;
};

struct DrawCommand {
    bool showVerts = true;
    bool showEdges = true;
    bool showFaces = true;

    std::vector<VertexHandle> highlightedVerts;
    std::vector<EdgeHandle> highlightedEdges;
    std::vector<FaceHandle> highlightedFaces;

    IMesh* mesh = nullptr;
    Mat4 model;
    Mat4 mvp;
};

struct DrawText3DCommand {
    const std::string& text;
    const Vec3& position;
    const Vec3& right;
    const Vec3& up;
    f32 size;
    const Mat4& mvp;
};

struct DrawGridCommand {
    Mat4 viewProjection;
    Vec3 cameraPosition;
    f32 cameraDistance;
    f32 farPlane;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(void* window, void* surface, const RendererConfig& config) = 0;
    virtual bool loadFonts(const FontLibrary& fonts) = 0;
    virtual void shutdown() = 0;

    virtual void resize(u32 width, u32 height) = 0;
    virtual void setVSync(bool enabled) = 0;
    virtual bool getVSync() const = 0;

    virtual void beginFrame() = 0;
    virtual void beginMainPass(const ClearState& clearState) = 0;
    virtual void setLighting(const LightingState& lighting) = 0;
    virtual void setBackground(const BackgroundGradient& background) = 0;
    virtual BackgroundGradient getBackground() const = 0;
    virtual void setBackFaceTint(const Vec3& tint) = 0;
    virtual Vec3 getBackFaceTint() const = 0;
    virtual void draw(const DrawCommand& command) = 0;
    virtual void drawText3D(const DrawText3DCommand& command) = 0;
    virtual void drawGrid(const DrawGridCommand& command) = 0;
    virtual void drawDebugLine(const Vec3& start, const Vec3& end, const Mat4& mvp) = 0;
    virtual void drawUI(const UIDrawList& list) = 0;
    virtual void endMainPass() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;

    virtual RendererBackend getBackend() const = 0;
    virtual const char* getBackendName() const = 0;
    ClearState m_clearState;
};

std::unique_ptr<IRenderer> createRenderer(RendererBackend backend);