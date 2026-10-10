#pragma once

// An OpenGL context on a window, with GL's functions loaded. The header has no platform types;
// opengl_context_win32.cpp is the Windows (WGL) side.
class OpenGLContext {
public:
    OpenGLContext() = default;
    ~OpenGLContext();

    OpenGLContext(const OpenGLContext&) = delete;
    OpenGLContext& operator=(const OpenGLContext&) = delete;

    // surface is the window's display context (Window::getNativeDisplayContext). Picks a pixel format, creates the
    // context, makes it current, and loads GL; false if any step fails, leaving nothing behind.
    bool create(void* surface);
    // Everything made with the context must be destroyed first
    void destroy();
    bool isCreated() const { return m_context != nullptr; }

    void setVSync(bool enabled);
    // Shows the frame (swaps the window's buffers)
    void present();

private:
    void* m_surface = nullptr;
    void* m_context = nullptr;
};
