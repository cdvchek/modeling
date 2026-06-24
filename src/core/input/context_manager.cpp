#include "core/input/context_manager.hpp"

void ContextManager::setContext(u32 ctx) {
    m_context = ctx |= InputContext_Global;
}

void ContextManager::addContext(u32 ctx) {
    m_context |= ctx;
}

void ContextManager::removeContext(u32 ctx) {
    ctx &= ~InputContext_Global;
    m_context &= ~ctx;
}

void ContextManager::toggleContext(u32 ctx) {
    ctx &= ~InputContext_Global;
    m_context ^= ctx;
}

bool ContextManager::isActive(u32 ctx) {
    return m_context & ctx;
}

u32 ContextManager::getContext() {
    return m_context;
}