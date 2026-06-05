#pragma once

#include <types>

enum class Key : u16 {
    Unknown = 0,

    A, B, C, D, E, F, G,
    H, I, J, K, L, M,
    N, O, P, Q, R, S, T,
    U, V, W, X, Y, Z,

    Num0, Num1, Num2, Num3, Num4,
    Num5, Num6, Num7, Num8, Num9,

    Escape,
    Enter,
    Tab,
    Backspace,
    Space,
    CapsLock,

    LeftShift, RightShift,
    LeftCtrl,  RightCtrl,
    LeftAlt,   RightAlt,
    LeftSuper, RightSuper,

    Comma, Period,
    Forwardslash, Backslash,
    SemiColon, Apostrophe,
    LeftBracket, RightBracket,
    Minus, Equals,

    Insert, Delete, Home, End,
    PageUp, PageDown,

    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,

    F1, F2, F3, F4, F5, F6,
    F7, F8, F9, F10, F11, F12,

    KeyPad0, KeyPad1, KeyPad2, KeyPad3,
    KeyPad4, KeyPad5, KeyPad6, KeyPad7,
    KeyPad8, KeyPad9,

    KeyPadAdd, KeyPadSub,
    KeyPadMult, KeyPadDivide,

    Count
};

enum class MouseButton : uint16_t {
    Unknown = 0,

    Left,
    Right,
    Middle,
    B4, B5, B6, B7, B8,
    
    Count
};

u16 translatePlatformKey(unsigned int code);