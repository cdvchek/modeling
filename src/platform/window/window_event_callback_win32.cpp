#include "platform/window/window_event_callback_win32.hpp"

#include "platform/window/impl_win32.hpp"
#include "platform/keys/keys.hpp"
#include "core/events/events.hpp"

unsigned int handleLeftRightKeys(unsigned int vk, LPARAM l_param) {
    if (vk == 16) { // VK_SHIFT
        UINT scan_code = (l_param >> 16) & 0xFF;
        vk = MapVirtualKey(scan_code, MAPVK_VSC_TO_VK_EX);
    }

    bool extended = (l_param & (1 << 24)) != 0;

    if (vk == 17) { // VK_CONTROL
        vk = extended ? 0xA3 /*Right control*/ : 0xA2; // Left control
    }

    if (vk == 12) { // Alt
        vk = extended ? 0xA5 /*Right alt*/ : 0xA4; // Left alt
    }

    return vk;
}

LRESULT WindowCallback::handleKeyDown(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        unsigned int vk = handleLeftRightKeys((UINT)w_param, l_param);
        u16 key = translatePlatformKey(vk);
        bool repeat = (l_param & (1 << 30)) != 0;
        impl->events->trigger(Event::KeyDown{ key, repeat });
    }

    return 0;
}

LRESULT WindowCallback::handleKeyUp(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        unsigned int vk = handleLeftRightKeys((UINT)w_param, l_param);
        u16 key = translatePlatformKey(vk);
        impl->events->trigger(Event::KeyUp{ key });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseMove(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseMove{ x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseLeftButtonDown(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = 1;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonDown{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseRightButtonDown(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = 2;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonDown{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseMiddleButtonDown(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = 3;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonDown{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseXButtonDown(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = GET_XBUTTON_WPARAM(w_param) == XBUTTON1 ? 4 : 5;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonDown{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseLeftButtonUp(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = 1;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonUp{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseRightButtonUp(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = 2;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonUp{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseMiddleButtonUp(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = 3;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonUp{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseXButtonUp(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        u16 button = GET_XBUTTON_WPARAM(w_param) == XBUTTON1 ? 4 : 5;
        i32 x = (i16)LOWORD(l_param);
        i32 y = (i16)HIWORD(l_param);

        impl->events->trigger(Event::MouseButtonUp{ button, x, y });
    }

    return 0;
}

LRESULT WindowCallback::handleMouseWheel(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        i32 delta = GET_WHEEL_DELTA_WPARAM(w_param);
        
        impl->events->trigger(Event::MouseWheel{ delta });
    }

    return 0;
}

LRESULT WindowCallback::handleClose(Window::Impl* impl, WPARAM w_param, LPARAM l_param) {
    if (impl && impl->events) {
        impl->events->trigger(Event::Quit{});
    }

    return 0;
}