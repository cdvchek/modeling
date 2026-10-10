#include "core/input/context_manager.hpp"

#include <utility>

ContextManager::ContextManager(u32 alwaysOn, std::vector<u32> modes) : m_alwaysOn(alwaysOn), m_modes(std::move(modes)) {
    for (u32 mode : m_modes) m_modeMask |= mode;

    if (!m_modes.empty()) m_modeContext = m_modes.front();
    m_context = m_alwaysOn | m_modeContext;
}

u32 ContextManager::pickMode(u32 ctx) const {
    for (u32 mode : m_modes) {
        if (ctx & mode) return mode;
    }
    return 0;
}

void ContextManager::setContext(u32 ctx) {
    // Only one mode can be active.
    const u32 mode = pickMode(ctx);

    // Remove all mode bits, then add back the chosen one.
    ctx &= ~m_modeMask;
    ctx |= mode;

    // Only remember the mode if one was provided.
    if (mode) {
        m_modeContext = mode;
    }

    m_context = ctx | m_alwaysOn;
}

void ContextManager::setModeContext(u32 mode) {
    mode = pickMode(mode);
    if (!mode) return;

    // Replace the current mode.
    m_context &= ~m_modeMask;
    m_context |= mode;

    m_modeContext = mode;
}

void ContextManager::addContext(u32 ctx) {
    // Modes must be changed through setModeContext().
    ctx &= ~m_modeMask;

    m_context |= ctx;
}

void ContextManager::removeContext(u32 ctx) {
    // Always-on contexts and modes can't be removed here.
    ctx &= ~(m_alwaysOn | m_modeMask);

    m_context &= ~ctx;
}

void ContextManager::toggleContext(u32 ctx) {
    // Always-on contexts and modes can't be toggled here.
    ctx &= ~(m_alwaysOn | m_modeMask);

    m_context ^= ctx;
}

bool ContextManager::isActive(u32 ctx) const {
    return (m_context & ctx) != 0;
}

u32 ContextManager::getModeContext() const {
    return m_modeContext;
}

u32 ContextManager::getContext() const {
    return m_context;
}
