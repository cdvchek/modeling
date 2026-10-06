#pragma once

#include <string>

namespace Platform {
    void pollEvents();
    void* getGLProcAddress(const char* name);

    // Plain text on the system clipboard; characters outside ASCII come back as '?'
    std::string getClipboardText();
    void setClipboardText(const std::string& text);
}