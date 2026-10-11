#pragma once

// An OpenGL context on a window, with GL's functions loaded; opengl_context_win32.cpp is the Windows (WGL) side
class OpenGLContext {
public:
    OpenGLContext() = default;
    ~OpenGLContext();

    OpenGLContext(const OpenGLContext&) = delete;
    OpenGLContext& operator=(const OpenGLContext&) = delete;

    // On the window's display context (Window::getNativeDisplayContext); false if any step fails, leaving nothing behind
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
