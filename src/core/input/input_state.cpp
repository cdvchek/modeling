#include "core/input/input_state.hpp"

#include <algorithm>
#include <iostream>

void InputState::beginFrame() {
    m_keys_was_down = m_keys_is_down;
    m_buttons_was_down = m_buttons_is_down;
    m_buttons_pressed.fill(false);
    m_buttons_released.fill(false);

    m_mouse_delta_x = 0;
    m_mouse_delta_y = 0;

    m_scroll = 0;

    m_keyPresses.clear();
    m_typed.clear();
}

void InputState::releaseAll() {
    m_keys_is_down.fill(false);
    m_keys_was_down.fill(false);
    m_buttons_is_down.fill(false);
    m_buttons_was_down.fill(false);
    m_buttons_pressed.fill(false);
    m_buttons_released.fill(false);
}

bool InputState::wasKeyPressedOrRepeated(u16 key) const {
    return std::find(m_keyPresses.begin(), m_keyPresses.end(), key) != m_keyPresses.end();
}

void InputState::onChar(char character) {
    // Control characters (Backspace, Enter, Ctrl+letter) arrive as key presses instead
    const unsigned char c = static_cast<unsigned char>(character);
    if (c >= 32 && c != 127) m_typed += character;
}

bool InputState::isKeyDown(u16 key) const {
    return m_keys_is_down[key];
}

bool InputState::wasKeyDown(u16 key) const {
    return m_keys_was_down[key];
}

bool InputState::wasKeyPressedThisFrame(u16 key) const {
    return (m_keys_is_down[key] && !m_keys_was_down[key]);
}

bool InputState::wasKeyReleasedThisFrame(u16 key) const {
    return (!m_keys_is_down[key] && m_keys_was_down[key]);
}

bool InputState::isMouseDown(u16 button) const {
    return m_buttons_is_down[button];
}

bool InputState::wasMouseDown(u16 button) const {
    return m_buttons_was_down[button];
}

bool InputState::wasMousePressedThisFrame(u16 button) const {
    return m_buttons_pressed[button];
}

bool InputState::wasMouseReleasedThisFrame(u16 button) const {
    return m_buttons_released[button];
}

i32 InputState::getMouseX() const {
    return m_mouse_x;
}

i32 InputState::getMouseY() const {
    return m_mouse_y;
}

i32 InputState::getMouseDeltaX() const {
    return m_mouse_delta_x;
}

i32 InputState::getMouseDeltaY() const {
    return m_mouse_delta_y;
}

i32 InputState::getScroll() const {
    return m_scroll;
}

void InputState::onKey(u16 key, bool pressed) {
    m_keys_is_down[key] = pressed;
    if (pressed) m_keyPresses.push_back(key);
}

void InputState::onMouseButton(u16 button, bool pressed) {
    if (pressed && !m_buttons_is_down[button]) m_buttons_pressed[button] = true;
    if (!pressed && m_buttons_is_down[button]) m_buttons_released[button] = true;
    m_buttons_is_down[button] = pressed;
}

void InputState::onScroll(i32 scroll) {
    m_scroll = scroll;
}

void InputState::onMouseMove(i32 x, i32 y) {
    // Added up: several moves can arrive in one frame, and a click reports its position too
    m_mouse_delta_x += x - m_mouse_x;
    m_mouse_delta_y += y - m_mouse_y;

    m_mouse_x = x;
    m_mouse_y = y;
}