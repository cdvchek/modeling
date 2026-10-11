#pragma once

#include <types>
#include <vector>

// Which input contexts are active. A context is a bit flag; each program defines its own.
class ContextManager {
public:
    ContextManager() = default;

    // alwaysOn never turns off; of modes only one is active at a time (the earliest wins), starting with the first
    ContextManager(u32 alwaysOn, std::vector<u32> modes);

    void setContext(u32 ctx);
    void setModeContext(u32 mode);

    void addContext(u32 ctx);
    void removeContext(u32 ctx);
    void toggleContext(u32 ctx);

    bool isActive(u32 ctx) const;

    // The mode last set, kept while a tool's context has replaced it
    u32 getModeContext() const;
    u32 getContext() const;

private:
    // The mode in ctx with the highest priority; 0 if it holds none
    u32 pickMode(u32 ctx) const;

    u32 m_alwaysOn = 0;
    std::vector<u32> m_modes;
    u32 m_modeMask = 0;

    u32 m_context = 0;
    u32 m_modeContext = 0;
};
