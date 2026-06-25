#pragma once

#include <vector>
#include <array>
#include <types>

#include "platform/keys/keys.hpp"

class EventDispatcher;

class InputState {
public: 
    void beginFrame();

    bool isKeyDown(u16 key) const;
    bool wasKeyDown(u16 key) const;
    bool wasKeyPressedThisFrame(u16 key) const;
    bool wasKeyReleasedThisFrame(u16 key) const;

    bool isMouseDown(u16 button) const;
    bool wasMouseDown(u16 button) const;
    bool wasMousePressedThisFrame(u16 button) const;
    bool wasMouseReleasedThisFrame(u16 button) const;

    i32 getMouseX() const;
    i32 getMouseY() const;
    i32 getMouseDeltaX() const;
    i32 getMouseDeltaY() const;
    i32 getScroll() const;

    void onKey(u16 key, bool pressed);
    void onMouseButton(u16 button, bool pressed);
    void onScroll(i32 scroll);
    void onMouseMove(i32 x, i32 y);

private:
    static constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t MOUSE_COUNT = static_cast<std::size_t>(MouseButton::Count);

    std::array<bool, KEY_COUNT> m_keys_is_down{};
    std::array<bool, KEY_COUNT> m_keys_was_down{};

    std::array<bool, MOUSE_COUNT> m_buttons_is_down{};
    std::array<bool, MOUSE_COUNT> m_buttons_was_down{};

    i32 m_mouse_x = 0;
    i32 m_mouse_y = 0;
    i32 m_mouse_delta_x = 0;
    i32 m_mouse_delta_y = 0;

    i32 m_scroll = 0;
};