#pragma once

#include <types>
#include <functional>
#include <array>
#include <vector>

#include "core/events/events.hpp"

class EventDispatcher {
public:
    template <typename Event>
    void trigger(const Event& event);

    template <typename Event>
    void subscribe(std::function<bool(const Event&)> callback);
private:
    static constexpr std::size_t MAX_EVENT_SIZE = 32;
    using Callback = std::function<bool(const void*)>;

    std::array<std::vector<Callback>, static_cast<u8>(EventType::Count)> m_callbacks;
};

#include "core/events/event_dispatcher.inl"