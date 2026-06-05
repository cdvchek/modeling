#include "renderer/renderer.hpp"
#include "renderer/opengl/opengl_renderer.hpp"

#include <memory>
#include <stdexcept>

std::unique_ptr<IRenderer> createRenderer(RendererBackend backend) {
    switch(backend) {
        case RendererBackend::RB_OpenGL: return std::make_unique<OpenGLRenderer>();
        case RendererBackend::RB_Vulkan:
        case RendererBackend::RB_DirectX12:
        case RendererBackend::RB_Metal:
        default:
            throw std::runtime_error("Requested renderer backend is not implemented.");
    }
}