#include "platform/window/window.hpp"

#include <windows.h>
#include "core/events/event_dispatcher.hpp"

struct Window::Impl {
    HWND hwnd;
    HDC hdc;
    HGLRC glrc;

    u32 width;
    u32 height;
    u32 position_x;
    u32 position_y;

    std::function<void()> onQuitRequested;

    EventDispatcher* events;
};