#pragma once

#include <types>
#include <memory>
#include <functional>
#include <string>

class EventDispatcher;

enum class CursorShape : u8 {
    Arrow,
    ResizeHorizontal,
    ResizeVertical,
    ResizeDiagonalDown,   // top-left to bottom-right
    ResizeDiagonalUp,     // bottom-left to top-right
    Text
};

class Window {

// Public methods return false or nullptr on error; see printWindowError.
public:
    struct Impl;
    Window() = delete;
    // name is the program's: the first title bar text and the window class, in UTF-8
    Window(u32 width, u32 height, const std::string& name);
    ~Window();

    bool initialize(EventDispatcher* event_dispatcher);

    bool getDimensions(u32& widthOut, u32& heightOut);
    bool getPosition(u32& posXOut, u32& posYOut);

    void* getNativeHandle();
    void* getNativeDisplayContext();
    
    void setOnQuitCallback(std::function<void()> onQuit);

    // Shown whenever the mouse is over the window's client area
    void setCursor(CursorShape shape);

    // The title bar text, in UTF-8
    void setTitle(const std::string& title);

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