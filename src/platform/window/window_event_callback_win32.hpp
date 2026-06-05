#pragma once

#include <windows.h>

#include "platform/window/window.hpp"

namespace WindowCallback {
    LRESULT handleKeyDown(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleKeyUp(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseMove(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseLeftButtonDown(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseRightButtonDown(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseMiddleButtonDown(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseXButtonDown(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseLeftButtonUp(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseRightButtonUp(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseMiddleButtonUp(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseXButtonUp(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleMouseWheel(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
    LRESULT handleClose(Window::Impl* impl, WPARAM wParam, LPARAM lParam);
};