#include "core/input/context_manager.hpp"

void ContextManager::setContext(u32 ctx) {
    u32 selection = ctx & SelectionMask;

    // Only one selection context can be active.
    // Priority: Vertex > Edge > Face.
    if (selection & InputContext_SelectionVertex) {
        selection = InputContext_SelectionVertex;
    } else if (selection & InputContext_SelectionEdge) {
        selection = InputContext_SelectionEdge;
    } else if (selection & InputContext_SelectionFace) {
        selection = InputContext_SelectionFace;
    }

    // Remove all selection bits, then add back the chosen one.
    ctx &= ~SelectionMask;
    ctx |= selection;

    // Only update selection history if a selection context was provided.
    if (selection) {
        m_selectionContext = selection;
    }

    // Global is always active.
    m_context = ctx | InputContext_Global;
}

void ContextManager::setSelectionContext(u32 selectionCtx) {
    selectionCtx &= SelectionMask;

    // Enforce exactly one selection context.
    // Priority: Vertex > Edge > Face.
    if (selectionCtx & InputContext_SelectionVertex) {
        selectionCtx = InputContext_SelectionVertex;
    } else if (selectionCtx & InputContext_SelectionEdge) {
        selectionCtx = InputContext_SelectionEdge;
    } else if (selectionCtx & InputContext_SelectionFace) {
        selectionCtx = InputContext_SelectionFace;
    } else {
        return;
    }

    // Replace the current selection context.
    m_context &= ~SelectionMask;
    m_context |= selectionCtx;

    // Remember the current selection mode.
    m_selectionContext = selectionCtx;
}

void ContextManager::addContext(u32 ctx) {
    // Selection contexts must be changed through setSelectionContext().
    ctx &= ~SelectionMask;

    m_context |= ctx;
}

void ContextManager::removeContext(u32 ctx) {
    // Global cannot be removed.
    // Selection contexts must be changed through setSelectionContext().
    ctx &= ~(InputContext_Global | SelectionMask);

    m_context &= ~ctx;
}

void ContextManager::toggleContext(u32 ctx) {
    // Global cannot be toggled.
    // Selection contexts must be changed through setSelectionContext().
    ctx &= ~(InputContext_Global | SelectionMask);

    m_context ^= ctx;
}

bool ContextManager::isActive(u32 ctx) const {
    return (m_context & ctx) != 0;
}

u32 ContextManager::getSelectionContext() const {
    return m_selectionContext;
}

u32 ContextManager::getContext() const {
    return m_context;
}