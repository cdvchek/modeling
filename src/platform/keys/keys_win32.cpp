#include "platform/keys/keys.hpp"

u16 translatePlatformKey(unsigned int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return ((u16)vk - (u16)'A') + (u16)Key::A;
    } else if (vk >= '0' && vk <= '9') {
        return ((u16)vk - (u16)'0') + (u16)Key::Num0;
    } else {
        switch(vk) {
            case 27:  return (u16)Key::Escape;               // VK_ESCAPE
            case 13:  return (u16)Key::Enter;                // VK_RETURN
            case 9:   return (u16)Key::Tab;                  // VK_TAB
            case 8:   return (u16)Key::Backspace;            // VK_BACK
            case 32:  return (u16)Key::Space;                // VK_SPACE
            case 20:  return (u16)Key::CapsLock;             // VK_CAPITAL

            case 160: return (u16)Key::LeftShift;            // VK_LSHIFT
            case 161: return (u16)Key::RightShift;           // VK_RSHIFT
            case 162: return (u16)Key::LeftCtrl;             // VK_LCONTROL
            case 163: return (u16)Key::RightCtrl;            // VK_RCONTROL
            case 164: return (u16)Key::LeftAlt;              // VK_LMENU
            case 165: return (u16)Key::RightAlt;             // VK_RMENU
            case 91:  return (u16)Key::LeftSuper;            // VK_LWIN
            case 92:  return (u16)Key::RightSuper;           // VK_RWIN

            case 188: return (u16)Key::Comma;                // VK_OEM_COMMA
            case 190: return (u16)Key::Period;               // VK_OEM_PERIOD
            case 191: return (u16)Key::Forwardslash;  // /?  // VK_OEM_2
            case 220: return (u16)Key::Backslash;     // \|  // VK_OEM_5
            case 186: return (u16)Key::SemiColon;     // ;:  // VK_OEM_1
            case 222: return (u16)Key::Apostrophe;    // '"  // VK_OEM_7
            case 219: return (u16)Key::LeftBracket;   // [{  // VK_OEM_4
            case 221: return (u16)Key::RightBracket;  // ]}  // VK_OEM_6
            case 189: return (u16)Key::Minus;         // -_  // VK_OEM_MINUS
            case 187: return (u16)Key::Equals;        // =+  // VK_OEM_PLUS

            case 45:  return (u16)Key::Insert;               // VK_INSERT
            case 46:  return (u16)Key::Delete;               // VK_DELETE
            case 36:  return (u16)Key::Home;                 // VK_HOME
            case 35:  return (u16)Key::End;                  // VK_END
            case 33:  return (u16)Key::PageUp;               // VK_PRIOR
            case 34:  return (u16)Key::PageDown;             // VK_NEXT

            case 38:  return (u16)Key::ArrowUp;              // VK_UP
            case 40:  return (u16)Key::ArrowDown;            // VK_DOWN
            case 37:  return (u16)Key::ArrowLeft;            // VK_LEFT
            case 39:  return (u16)Key::ArrowRight;           // VK_RIGHT

            case 112: return (u16)Key::F1;                   // VK_F1
            case 113: return (u16)Key::F2;                   // VK_F2
            case 114: return (u16)Key::F3;                   // VK_F3
            case 115: return (u16)Key::F4;                   // VK_F4
            case 116: return (u16)Key::F5;                   // VK_F5
            case 117: return (u16)Key::F6;                   // VK_F6
            case 118: return (u16)Key::F7;                   // VK_F7
            case 119: return (u16)Key::F8;                   // VK_F8
            case 120: return (u16)Key::F9;                   // VK_F9
            case 121: return (u16)Key::F10;                  // VK_F10
            case 122: return (u16)Key::F11;                  // VK_F11
            case 123: return (u16)Key::F12;                  // VK_F12

            case 96:  return (u16)Key::KeyPad0;              // VK_NUMPAD0
            case 97:  return (u16)Key::KeyPad1;              // VK_NUMPAD1
            case 98:  return (u16)Key::KeyPad2;              // VK_NUMPAD2
            case 99:  return (u16)Key::KeyPad3;              // VK_NUMPAD3
            case 100: return (u16)Key::KeyPad4;              // VK_NUMPAD4
            case 101: return (u16)Key::KeyPad5;              // VK_NUMPAD5
            case 102: return (u16)Key::KeyPad6;              // VK_NUMPAD6
            case 103: return (u16)Key::KeyPad7;              // VK_NUMPAD7
            case 104: return (u16)Key::KeyPad8;              // VK_NUMPAD8
            case 105: return (u16)Key::KeyPad9;              // VK_NUMPAD9

            case 107: return (u16)Key::KeyPadAdd;            // VK_ADD
            case 109: return (u16)Key::KeyPadSub;            // VK_SUBTRACT
            case 106: return (u16)Key::KeyPadMult;           // VK_MULTIPLY
            case 111: return (u16)Key::KeyPadDivide;         // VK_DIVIDE

            default:  return (u16)Key::Unknown;
        }
    }
}