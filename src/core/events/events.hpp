#pragma once

#include <types>

#define EVENT_TYPE(event_type) \
    static constexpr EventType type = EventType::event_type; \
    static constexpr const char* name = #event_type

enum class EventType : u8 {
    KeyDown,
    KeyUp,
    Char,
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel,
    WindowResize,
    Quit,

    Count
};

namespace Event {
    struct KeyDown { EVENT_TYPE(KeyDown);
        u16 key;
        bool repeat;
    };

    struct KeyUp { EVENT_TYPE(KeyUp);
        u16 key;
    };

    struct Char { EVENT_TYPE(Char);
        char character;
    };

    struct MouseMove { EVENT_TYPE(MouseMove);
        i32 x;
        i32 y;
    };

    struct MouseButtonDown { EVENT_TYPE(MouseButtonDown);
        u16 button;
        i32 x;
        i32 y;
    };

    struct MouseButtonUp { EVENT_TYPE(MouseButtonUp);
        u16 button;
        i32 x;
        i32 y;
    };

    struct MouseWheel { EVENT_TYPE(MouseWheel);
        i32 delta;
    };

    // Client area size in pixels. Both are 0 while the window is minimized.
    struct WindowResize { EVENT_TYPE(WindowResize);
        u32 width;
        u32 height;
    };

    struct Quit { EVENT_TYPE(Quit); };
}

#undef EVENT_TYPE