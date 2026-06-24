#pragma once

#include "core/input/contexts.hpp"

class ContextManager {
public:
    void setContext(u32 ctx);
    void addContext(u32 ctx);
    void removeContext(u32 ctx);
    void toggleContext(u32 ctx);
    bool isActive(u32 ctx);
    u32 getContext();
private:
    u32 m_context = InputContext_Global;
};