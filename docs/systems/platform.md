# Platform

Win32-specific code: the window, the OS message pump, key translation, and OpenGL context setup. Everything here ends in `_win32.cpp`; the headers are platform-neutral.

Files: `src/platform/`

## Platform namespace

[platform.hpp](../../src/platform/platform.hpp)

| Function | Description |
|---|---|
| `void Platform::pollEvents()` | Drains the Win32 message queue (`PeekMessage` / `DispatchMessage`). Called once per frame. Messages reach the window procedure, which turns them into events. |
| `std::string Platform::getClipboardText()` / `void Platform::setClipboardText(text)` | Plain text on the system clipboard (`CF_UNICODETEXT`). Characters outside ASCII come back as `?`. Text fields use them through `UIContext::setClipboard`. |
| `chooseOpenFile(window, typeName, extension, folder)` / `chooseSaveFile(window, typeName, extension, folder, fileName)` | The Windows Open and Save As dialogs (`GetOpenFileNameW` / `GetSaveFileNameW` from `comdlg32`), filtered to `typeName (*.ext)` plus All files. They start in `folder` (Windows may pick the folder you last browsed to instead, by its own remembered-folder rule). Save starts with `fileName`, adds the extension when it's left off, and asks before overwriting. `OFN_NOCHANGEDIR` keeps the app's working folder. Return an empty path when cancelled. |
| `documentsFolder()` | The user's Documents folder from `SHGetKnownFolderPath(FOLDERID_Documents)` (follows OneDrive redirection); empty if it can't be found. |
| `askToSaveChanges(window, title, name)` | A Yes/No/Cancel box, "Save changes to name?", returned as `SaveChoice::Save` / `DontSave` / `Cancel`. |
| `void* Platform::getGLProcAddress(const char* name)` | Looks up an OpenGL function with `wglGetProcAddress`, falling back to `opengl32.dll` for GL 1.1 functions. |

## Window

[window.hpp](../../src/platform/window/window.hpp), [window_win32.cpp](../../src/platform/window/window_win32.cpp)

Pimpl class: the header has no Win32 types; `Window::Impl` (in `impl_win32.hpp`) holds the `HWND`, `HDC`, and a pointer to the `EventDispatcher`.

| Method | Description |
|---|---|
| `Window(u32 width, u32 height)` | Stores the requested size. The app creates a 1920×1080 window. |
| `bool initialize(EventDispatcher*)` | Registers the window class, creates an `WS_OVERLAPPEDWINDOW` window, and keeps the dispatcher so the window procedure can trigger events. |
| `bool getDimensions(u32& w, u32& h)` | Current client-area size. Used every frame for aspect ratio and mouse-ray math. |
| `bool getPosition(u32& x, u32& y)` | Window position on screen. |
| `void* getNativeHandle()` / `getNativeDisplayContext()` | `HWND` / `HDC`, passed to the renderer to create the GL context. |
| `void setCursor(CursorShape)` | Cursor shown over the client area: `Arrow`, `ResizeHorizontal`, `ResizeVertical`, `ResizeDiagonalDown`, `ResizeDiagonalUp`, `Text` (I-beam). Applied immediately when the mouse is over the window (or captured), and on every `WM_SETCURSOR` in the client area. The app sets it each frame from `ctx.ui.cursor()`. |
| `void setTitle(const std::string&)` | The title bar text (UTF-8). The window starts as "Valuma Studio"; the app keeps it as `name.vlm* - Valuma Studio` (see [project.md](project.md)). |
| `void printWindowError() const` | Prints the last `WindowError`. Public methods return `false`/`nullptr` on failure. |

### Window procedure

[window_event_callback_win32.cpp](../../src/platform/window/window_event_callback_win32.cpp) has one `WindowCallback::handle*` function per message. Each translates the message and calls `dispatcher->trigger(Event::...)`:

| Win32 message | Event |
|---|---|
| `WM_KEYDOWN`, `WM_SYSKEYDOWN` | `Event::KeyDown` |
| `WM_KEYUP`, `WM_SYSKEYUP` | `Event::KeyUp` |
| `WM_CHAR` | `Event::Char` |
| `WM_MOUSEMOVE` | `Event::MouseMove` |
| `WM_[L/R/M/X]BUTTONDOWN` / `UP` | `Event::MouseButtonDown` / `Up` |
| `WM_CAPTURECHANGED` | `Event::MouseButtonUp` for each of left/right/middle that isn't physically held (`GetKeyState`) |
| `WM_MOUSEWHEEL` | `Event::MouseWheel` |
| `WM_SIZE` | `Event::WindowResize` |
| `WM_CLOSE` | `Event::Quit` |

**Mouse capture:** `windowProc` calls `SetCapture` on any button down and `ReleaseCapture` once no button is held. While captured, moves and releases outside the window still arrive (coordinates can be negative or past the edge), so a drag that ends outside the window doesn't leave a button stuck down. If capture is taken away mid-drag (Alt+Tab), `WM_CAPTURECHANGED` releases the buttons that are no longer held.

## Keys

[keys.hpp](../../src/platform/keys/keys.hpp), [keys_win32.cpp](../../src/platform/keys/keys_win32.cpp)

`enum class Key : u16` and `enum class MouseButton` are the app's own key codes. `keys_win32.cpp` maps Win32 virtual-key codes to them. Left/right modifier keys are separate (`LeftShift`, `RightShift`, …). `Key::Count` sizes the arrays in `InputState`.

To support a new key: add it to `Key` and add its virtual-key mapping in `keys_win32.cpp`.

## OpenGL context

[opengl_renderer_win32.cpp](../../src/platform/renderer/opengl_renderer_win32.cpp) implements `OpenGLRenderer::initialize`, `shutdown`, `setVSync`, and `present` with WGL. It picks a pixel format, creates a legacy (compatibility) context with `wglCreateContext`, loads GL with GLAD, then calls `createResources()` (in the portable renderer code) for shaders and shared buffers. `shutdown` calls `destroyResources()` before deleting the context. See [renderer.md](renderer.md).
