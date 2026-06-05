#pragma once

namespace Platform {
    void pollEvents();
    void* getGLProcAddress(const char* name);
}