template <typename Event>
void EventDispatcher::trigger(const Event& event) {
    static_assert(sizeof(Event) <= MAX_EVENT_SIZE, "Event is too large");

    auto& callbacks= m_callbacks[static_cast<u8>(Event::type)];

    for (auto& callback : callbacks) {
        if (callback(&event)) break;
    }
}

template <typename Event>
void EventDispatcher::subscribe(std::function<bool(const Event&)> callback) {
    static_assert(sizeof(Event) <= MAX_EVENT_SIZE, "Event is too large");

    auto wrapper = [callback](const void* event) -> bool {
        return callback(*static_cast<const Event*>(event));
    };

    auto& callbacks = m_callbacks[static_cast<u8>(Event::type)];
    callbacks.push_back(wrapper);
}