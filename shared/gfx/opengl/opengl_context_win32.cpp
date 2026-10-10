#include "gfx/opengl/opengl_context.hpp"

#include <windows.h>
#include <glad/glad.h>

OpenGLContext::~OpenGLContext() { destroy(); }

bool OpenGLContext::create(void* surface) {
    if (m_context) return true;
    if (surface == nullptr) return false;

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

    if (!wglMakeCurrent(w32Surface, w32glContext)) {
        wglDeleteContext(w32glContext);
        return false;
    }

    // GL isn't usable until its functions load and it reports a version
    if (!gladLoadGL() || !glGetString(GL_VERSION)) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(w32glContext);
        return false;
    }

    m_surface = surface;
    m_context = static_cast<void*>(w32glContext);
    return true;
}

void OpenGLContext::destroy() {
    HGLRC w32glContext = static_cast<HGLRC>(m_context);

    if (w32glContext) {
        if (wglGetCurrentContext() == w32glContext) {
            wglMakeCurrent(nullptr, nullptr);
        }

        wglDeleteContext(w32glContext);
    }

    m_context = nullptr;
    m_surface = nullptr;
}

void OpenGLContext::setVSync(bool enabled) {
    if (!m_context) return;

    using PFNWGLSWAPINTERVALEXTPROC = BOOL(WINAPI*)(int);

    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(
            wglGetProcAddress("wglSwapIntervalEXT")
        );

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(enabled ? 1 : 0);
    }
}

void OpenGLContext::present() {
    if (m_surface == nullptr) return;

    HDC w32surface = static_cast<HDC>(m_surface);
    SwapBuffers(w32surface);
}
