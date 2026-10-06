#include "platform/platform.hpp"

#include <windows.h>

void Platform::pollEvents() {
    static MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void* Platform::getGLProcAddress(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (p == 0 || (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) || (p == (void*)-1)) {
        HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void *)GetProcAddress(module, name);
    }
    
    return p;
}
std::string Platform::getClipboardText() {
    std::string text;
    if (!OpenClipboard(nullptr)) return text;

    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(data))) {
            for (const wchar_t* c = wide; *c; ++c) text += *c < 128 ? static_cast<char>(*c) : '?';
            GlobalUnlock(data);
        }
    }

    CloseClipboard();
    return text;
}

void Platform::setClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    // The clipboard takes ownership of the memory once SetClipboardData succeeds
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
        wchar_t* wide = static_cast<wchar_t*>(GlobalLock(memory));
        for (std::size_t i = 0; i < text.size(); ++i) wide[i] = static_cast<unsigned char>(text[i]);
        wide[text.size()] = 0;
        GlobalUnlock(memory);

        if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    }

    CloseClipboard();
}
