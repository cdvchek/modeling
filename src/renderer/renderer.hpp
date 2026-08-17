#pragma once

#include <types>
#include <cstdint>
#include <memory>
#include <vector>

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
    std::vector<u32> selected;
    // first entry is the number of highlighted verts, we will call it "x"
    // second entry is the number of highlighted edges, we will call it "y"
    // third entry is the number of highlighted faces, we will call it "z"
    // the following "x" number of entries will be the indices of the highlighted verts
    // after that, the following "y" number of entries will be the indices of the highlighted edges
    // after that, the following "z" number of entries will be the indicis of the highlighted faces
    
    IMesh* mesh = nullptr;
    const std::vector<u32> selected;
    Mat4 mvp;
};

struct PointDrawCommand {
    Vec3 position;
    Mat4 viewProjection;
    Vec3 color = Vec3(1.0f, 0.8f, 0.0f);
    f32 size = 12.0f;
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
    virtual void drawPoint(const PointDrawCommand& comand) = 0;
    virtual void endMainPass() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;

    virtual RendererBackend getBackend() const = 0;
    virtual const char* getBackendName() const = 0;
    ClearState m_clearState;
};

std::unique_ptr<IRenderer> createRenderer(RendererBackend backend);