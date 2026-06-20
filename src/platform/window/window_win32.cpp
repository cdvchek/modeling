#include "platform/window/window.hpp"

#include <windows.h>

#include "platform/window/window_event_callback_win32.hpp"
#include "platform/window/impl_win32.hpp"

#define WINDOW_REQUIRE_INITIALIZED_BOOL() \
    if (!isInitialized()) {          \
        m_error = WE_NOTINITIALIZED; \
        return false;                \
    }

#define WINDOW_REQUIRE_INITIALIZED_PTR() \
    if (!isInitialized()) {          \
        m_error = WE_NOTINITIALIZED; \
        return nullptr;              \
    }

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM w_param, LPARAM l_param);

Window::Window(u32 width, u32 height) {
    m_impl = std::make_unique<Impl>();
    m_impl->width = width;
    m_impl->height = height;
}

Window::~Window() = default;

bool Window::initialize(EventDispatcher* event_dispatcher) {
    const wchar_t class_name[] = L"Modeling";
    HINSTANCE h_instance = GetModuleHandle(nullptr);
    WNDCLASSW wc = {};

    wc.lpfnWndProc = windowProc;
    wc.hInstance = h_instance;
    wc.lpszClassName = class_name;

    RegisterClassW(&wc);

    m_impl->events = event_dispatcher;

    HWND hwnd = CreateWindowExW(
        0, // Option window styles
        class_name, // Window class
        L"Modeling", // Window text
        WS_OVERLAPPEDWINDOW, // Window style

        // Position and size
        CW_USEDEFAULT, CW_USEDEFAULT, (int)m_impl->width, (int)m_impl->height,

        NULL, // Parent window
        NULL, // Menu
        h_instance,
        m_impl.get() // Additional application data
    );

    if (hwnd == NULL) {
        DWORD err = GetLastError();
        m_error = WE_HWNDFAIL;
        return false;
    }

    m_impl->hwnd = hwnd;
    m_impl->hdc = GetDC(hwnd);

    ShowWindow(hwnd, SW_SHOW);

    m_initialized = true;
    return true;
}

bool Window::getDimensions(u32& widthOut, u32& heightOut) {
    WINDOW_REQUIRE_INITIALIZED_BOOL();

    RECT rect;
    GetClientRect(m_impl->hwnd, &rect);

    widthOut = static_cast<u32>(rect.right - rect.left);
    heightOut = static_cast<u32>(rect.bottom - rect.top);

    return true;
}

bool Window::getPosition(u32& posXOut, u32& posYOut) {
    WINDOW_REQUIRE_INITIALIZED_BOOL();

    posXOut = m_impl->position_x;
    posYOut = m_impl->position_y;

    return true;
}

void* Window::getNativeHandle() {
    WINDOW_REQUIRE_INITIALIZED_PTR();

    return (void*)m_impl->hwnd;
}

void* Window::getNativeDisplayContext() {
    WINDOW_REQUIRE_INITIALIZED_PTR();

    return (void*)m_impl->hdc;
}

void Window::setOnQuitCallback(std::function<void()> onQuit) {
    m_impl->onQuitRequested = onQuit;
}

void Window::printWindowError() const {
    switch(m_error) {
        case WE_NOERROR:
            return;
        case WE_NOTINITIALIZED:
            return;
        case WE_HWNDFAIL:
            return;
    }
}

bool Window::isInitialized() const {
    return m_initialized;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* impl = reinterpret_cast<Window::Impl*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    Window::Impl* impl = reinterpret_cast<Window::Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (uMsg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            return WindowCallback::handleKeyDown(impl, wParam, lParam);
        case WM_KEYUP:
        case WM_SYSKEYUP:
            return WindowCallback::handleKeyUp(impl, wParam, lParam);
        case WM_MOUSEMOVE:
            return WindowCallback::handleMouseMove(impl, wParam, lParam);
        case WM_LBUTTONDOWN:
            return WindowCallback::handleMouseLeftButtonDown(impl, wParam, lParam);
        case WM_RBUTTONDOWN:
            return WindowCallback::handleMouseRightButtonDown(impl, wParam, lParam);
        case WM_MBUTTONDOWN:
            return WindowCallback::handleMouseMiddleButtonDown(impl, wParam, lParam);
        case WM_XBUTTONDOWN:
            return WindowCallback::handleMouseXButtonDown(impl, wParam, lParam);
        case WM_LBUTTONUP:
            return WindowCallback::handleMouseLeftButtonUp(impl, wParam, lParam);
        case WM_RBUTTONUP:
            return WindowCallback::handleMouseRightButtonUp(impl, wParam, lParam);
        case WM_MBUTTONUP:
            return WindowCallback::handleMouseMiddleButtonUp(impl, wParam, lParam);
        case WM_XBUTTONUP:
            return WindowCallback::handleMouseXButtonUp(impl, wParam, lParam);
        case WM_MOUSEWHEEL:
            return WindowCallback::handleMouseWheel(impl, wParam, lParam);
        case WM_CLOSE:
            return WindowCallback::handleClose(impl, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}