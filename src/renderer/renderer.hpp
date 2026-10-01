#pragma once

#include <types>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"
#include "renderer/gpu_mesh.hpp"

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

struct DrawCommand {
    bool showVerts = true;
    bool showEdges = true;
    bool showFaces = true;

    std::vector<VertexHandle> highlightedVerts;
    std::vector<EdgeHandle> highlightedEdges;
    std::vector<FaceHandle> highlightedFaces;

    IMesh* mesh = nullptr;
    Mat4 mvp;
};

struct DrawTextCommand {
    const std::string& text;
    f32 x;
    f32 y;
    Vec3 color;
};

struct DrawText3DCommand {
    const std::string& text;
    const Vec3& position;
    const Vec3& right;
    const Vec3& up;
    f32 size;
    const Mat4& mvp;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(void* window, void* surface, const RendererConfig& config) = 0;
    virtual void shutdown() = 0;

    virtual void resize(u32 width, u32 height) = 0;
    virtual void setVSync(bool enabled) = 0;

    virtual void beginFrame() = 0;
    virtual void beginMainPass(const ClearState& clearState) = 0;
    virtual void draw(const DrawCommand& command) = 0;
    virtual void drawText(const DrawTextCommand& command) = 0;
    virtual void drawText3D(const DrawText3DCommand& command) = 0;
    virtual void drawDebugLine(const Vec3& start, const Vec3& end, const Mat4& mvp) = 0;
    virtual void drawConsoleBackground() = 0;
    virtual void endMainPass() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;

    virtual RendererBackend getBackend() const = 0;
    virtual const char* getBackendName() const = 0;
    ClearState m_clearState;
};

std::unique_ptr<IRenderer> createRenderer(RendererBackend backend);