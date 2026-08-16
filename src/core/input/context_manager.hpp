#pragma once

#include "core/input/contexts.hpp"

class ContextManager {
public:
    void setContext(u32 ctx);
    void setSelectionContext(u32 selectionCtx);

    void addContext(u32 ctx);
    void removeContext(u32 ctx);
    void toggleContext(u32 ctx);

    bool isActive(u32 ctx) const;

    u32 getSelectionContext() const;
    u32 getContext() const;

private:
    static constexpr u32 SelectionMask =
        InputContext_SelectionVertex |
        InputContext_SelectionEdge |
        InputContext_SelectionFace;

    u32 m_context =
        InputContext_Global |
        InputContext_SelectionVertex;

    u32 m_selectionContext =
        InputContext_SelectionVertex;
};