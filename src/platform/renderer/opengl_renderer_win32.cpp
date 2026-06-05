#include "renderer/opengl/opengl_renderer.hpp"

#include <stdexcept>
#include <windows.h>
#include <glad/glad.h>

bool OpenGLRenderer::initialize(void* window, void* surface, const RendererConfig& config) {   
    if (m_initialized) return true;
    if (window == nullptr) return false;
    if (surface == nullptr) return false;

    m_width = config.width;
    m_height = config.height;

    m_window = window;
    m_surface = surface;

    m_vsyncEnabled = config.enableVSync;

    HDC w32Surface = static_cast<HDC>(surface);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    
    int pixelFormat = ChoosePixelFormat(w32Surface, &pfd);
    if (pixelFormat == 0) {
        return false;
    }
    
    if (!SetPixelFormat(w32Surface, pixelFormat, &pfd)) {
        return false;
    }
    
    HGLRC w32glContext = wglCreateContext(w32Surface);

    if (!w32glContext) {
        return false;
    }

    m_glContext = static_cast<void*>(w32glContext);
    
    if (!wglMakeCurrent(w32Surface, w32glContext)) {
        wglDeleteContext(w32glContext);
        m_glContext = nullptr;
        return false;
    }
    
    if (!gladLoadGL()) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(w32glContext);
        m_glContext = nullptr;
        return false;
    }
    
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!version) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(w32glContext);
        m_glContext = nullptr;
        return false;
    }

    glViewport(0, 0, static_cast<GLsizei>(config.width), static_cast<GLsizei>(config.height));
    glEnable(GL_DEPTH_TEST);

    setVSync(config.enableVSync);

    m_initialized = true;
    return true;
}

void OpenGLRenderer::shutdown() {
    if (!m_initialized) return;

    HGLRC w32glContext = static_cast<HGLRC>(m_glContext);

    if (w32glContext) {
        if (wglGetCurrentContext() == w32glContext) {
            wglMakeCurrent(nullptr, nullptr);
        }

        wglDeleteContext(w32glContext);
    }

    m_glContext = nullptr;
    m_window = nullptr;
    m_surface = nullptr;
    m_initialized = false;
}

void OpenGLRenderer::setVSync(bool enabled) {
    m_vsyncEnabled = enabled;

    using PFNWGLSWAPINTERVALEXTPROC = BOOL(WINAPI*)(int);

    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(
            wglGetProcAddress("wglSwapIntervalEXT")
        );

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(enabled ? 1 : 0);
    }
}

void OpenGLRenderer::present() {
    if (!m_initialized) return;
    if (m_surface == nullptr) return;

    HDC w32surface = static_cast<HDC>(m_surface);
    SwapBuffers(w32surface);
}