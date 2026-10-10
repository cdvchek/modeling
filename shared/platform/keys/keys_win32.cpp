#include "platform/keys/platform_keys.hpp"
#include <Windows.h>

u16 translatePlatformKey(unsigned int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return ((u16)vk - (u16)'A') + (u16)Key::A;
    } else if (vk >= '0' && vk <= '9') {
        return ((u16)vk - (u16)'0') + (u16)Key::Num0;
    } else {
        switch(vk) {
            case VK_ESCAPE:      return (u16)Key::Escape;
            case VK_RETURN:      return (u16)Key::Enter;
            case VK_TAB:         return (u16)Key::Tab;
            case VK_BACK:        return (u16)Key::Backspace;
            case VK_SPACE:       return (u16)Key::Space;
            case VK_CAPITAL:     return (u16)Key::CapsLock;

            case VK_LSHIFT:      return (u16)Key::LeftShift;
            case VK_RSHIFT:      return (u16)Key::RightShift;
            case VK_LCONTROL:    return (u16)Key::LeftCtrl;
            case VK_RCONTROL:    return (u16)Key::RightCtrl;
            case VK_LMENU:       return (u16)Key::LeftAlt;
            case VK_RMENU:       return (u16)Key::RightAlt;
            case VK_LWIN:        return (u16)Key::LeftSuper;
            case VK_RWIN:        return (u16)Key::RightSuper;

            case VK_OEM_COMMA:   return (u16)Key::Comma;
            case VK_OEM_PERIOD:  return (u16)Key::Period;
            case VK_OEM_2:       return (u16)Key::Forwardslash;
            case VK_OEM_5:       return (u16)Key::Backslash;
            case VK_OEM_1:       return (u16)Key::SemiColon;
            case VK_OEM_7:       return (u16)Key::Apostrophe;
            case VK_OEM_4:       return (u16)Key::LeftBracket;
            case VK_OEM_6:       return (u16)Key::RightBracket;
            case VK_OEM_MINUS:   return (u16)Key::Minus;
            case VK_OEM_PLUS:    return (u16)Key::Equals;
            case VK_OEM_3:       return (u16)Key::Tilde;

            case VK_INSERT:      return (u16)Key::Insert;
            case VK_DELETE:      return (u16)Key::Delete;
            case VK_HOME:        return (u16)Key::Home;
            case VK_END:         return (u16)Key::End;
            case VK_PRIOR:       return (u16)Key::PageUp;
            case VK_NEXT:        return (u16)Key::PageDown;

            case VK_UP:          return (u16)Key::ArrowUp;
            case VK_DOWN:        return (u16)Key::ArrowDown;
            case VK_LEFT:        return (u16)Key::ArrowLeft;
            case VK_RIGHT:       return (u16)Key::ArrowRight;

            case VK_F1:          return (u16)Key::F1;
            case VK_F2:          return (u16)Key::F2;
            case VK_F3:          return (u16)Key::F3;
            case VK_F4:          return (u16)Key::F4;
            case VK_F5:          return (u16)Key::F5;
            case VK_F6:          return (u16)Key::F6;
            case VK_F7:          return (u16)Key::F7;
            case VK_F8:          return (u16)Key::F8;
            case VK_F9:          return (u16)Key::F9;
            case VK_F10:         return (u16)Key::F10;
            case VK_F11:         return (u16)Key::F11;
            case VK_F12:         return (u16)Key::F12;

            case VK_NUMPAD0:     return (u16)Key::KeyPad0;
            case VK_NUMPAD1:     return (u16)Key::KeyPad1;
            case VK_NUMPAD2:     return (u16)Key::KeyPad2;
            case VK_NUMPAD3:     return (u16)Key::KeyPad3;
            case VK_NUMPAD4:     return (u16)Key::KeyPad4;
            case VK_NUMPAD5:     return (u16)Key::KeyPad5;
            case VK_NUMPAD6:     return (u16)Key::KeyPad6;
            case VK_NUMPAD7:     return (u16)Key::KeyPad7;
            case VK_NUMPAD8:     return (u16)Key::KeyPad8;
            case VK_NUMPAD9:     return (u16)Key::KeyPad9;

            case VK_ADD:         return (u16)Key::KeyPadAdd;
            case VK_SUBTRACT:    return (u16)Key::KeyPadSub;
            case VK_MULTIPLY:    return (u16)Key::KeyPadMult;
            case VK_DIVIDE:      return (u16)Key::KeyPadDivide;

            default:  return (u16)Key::Unknown;
        }
    }
}