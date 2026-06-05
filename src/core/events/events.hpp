#pragma once

#include <types>

#define EVENT_TYPE(event_type) \
    static constexpr EventType type = EventType::event_type; \
    static constexpr const char* name = #event_type

enum class EventType : u8 {
    KeyDown,
    KeyUp,
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel,
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

    struct Quit { EVENT_TYPE(Quit); };
}

#undef EVENT_TYPE