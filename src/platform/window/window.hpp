#pragma once

#include <types>
#include <memory>
#include <functional>

class EventDispatcher;

class Window {

// All public methods return false or nullptr to indicate an error occurred.
// Use printWindowError to see what the last error was.
public:
    struct Impl;
    Window() = delete;
    Window(u32 width, u32 height);
    ~Window();

    bool initialize(EventDispatcher* event_dispatcher);

    bool getDimensions(u32& widthOut, u32& heightOut);
    bool getPosition(u32& posXOut, u32& posYOut);

    void* getNativeHandle();
    void* getNativeDisplayContext();
    
    void setOnQuitCallback(std::function<void()> onQuit);

    void printWindowError() const;

private:
    bool isInitialized() const;
    bool m_initialized = false;

    std::unique_ptr<Impl> m_impl;
    
    enum WindowError : u8 {
        WE_NOERROR,
        WE_NOTINITIALIZED,
        WE_HWNDFAIL,
    };

    WindowError m_error;
};