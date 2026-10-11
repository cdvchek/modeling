#include "editor/editor.hpp"

#include <chrono>
#include <glad/glad.h>

#include "platform/platform.hpp"

namespace {
    constexpr const char* APP_NAME = "Aevora Engine";

    CursorShape toCursorShape(UICursor cursor) {
        switch (cursor) {
            case UICursor::ResizeHorizontal: return CursorShape::ResizeHorizontal;
            case UICursor::ResizeVertical: return CursorShape::ResizeVertical;
            case UICursor::ResizeDiagonalDown: return CursorShape::ResizeDiagonalDown;
            case UICursor::ResizeDiagonalUp: return CursorShape::ResizeDiagonalUp;
            case UICursor::Text: return CursorShape::Text;
            default: return CursorShape::Arrow;
        }
    }
}

const char* workspaceName(Workspace workspace) {
    switch (workspace) {
        case Workspace::World: return "World";
        case Workspace::Scene: return "Scene";
        case Workspace::Data: return "Data";
        default: return "";
    }
}

bool Editor::initialize(EditorContext& ctx) {
    ctx.window = std::make_unique<Window>(1920, 1080, APP_NAME);
    if (!ctx.window->initialize(&ctx.events)) return false;
    ctx.window->getDimensions(ctx.width, ctx.height);

    if (!ctx.graphics.create(ctx.window->getNativeDisplayContext())) return false;
    ctx.graphics.setVSync(true);

    if (!ctx.fonts.loadEmbedded()) return false;
    for (u32 i = 0; i < ctx.fontTextures.size(); ++i) {
        if (!ctx.fontTextures[i].create(ctx.fonts.get(static_cast<FontId>(i)))) return false;
    }
    if (!ctx.uiRenderer.create()) return false;

    registerEvents(ctx);
    registerActions(ctx);
    registerCommands(ctx);
    ctx.ui.setClipboard(&Platform::getClipboardText, &Platform::setClipboardText);
    ctx.projectsFolder = Platform::documentsFolder();
    updateWindowTitle(ctx);
    return true;
}

void Editor::shutdown(EditorContext& ctx) {
    ctx.uiRenderer.destroy();
    for (OpenGLFont& font : ctx.fontTextures) font.destroy();
    ctx.graphics.destroy();
}

void Editor::registerEvents(EditorContext& ctx) {
    ctx.events.subscribe<Event::KeyDown>([&ctx](const Event::KeyDown& event) {
        ctx.input.onKey(event.key, true);
        return false;
    });
    ctx.events.subscribe<Event::KeyUp>([&ctx](const Event::KeyUp& event) {
        ctx.input.onKey(event.key, false);
        return false;
    });
    ctx.events.subscribe<Event::Char>([&ctx](const Event::Char& event) {
        ctx.input.onChar(event.character);

        // '/' opens and closes the console, so it never ends up in the command
        const char c = event.character;
        if (ctx.contexts.isActive(InputContext_Console) && c != '\b' && c != '\r' && c != '\t' && c != '/') ctx.console.insertToCurrentCommand(c);
        return false;
    });
    ctx.events.subscribe<Event::MouseMove>([&ctx](const Event::MouseMove& event) {
        ctx.input.onMouseMove(event.x, event.y);
        return false;
    });
    ctx.events.subscribe<Event::MouseButtonDown>([&ctx](const Event::MouseButtonDown& event) {
        ctx.input.onMouseMove(event.x, event.y);
        ctx.input.onMouseButton(event.button, true);
        return false;
    });
    ctx.events.subscribe<Event::MouseButtonUp>([&ctx](const Event::MouseButtonUp& event) {
        ctx.input.onMouseMove(event.x, event.y);
        ctx.input.onMouseButton(event.button, false);
        return false;
    });
    ctx.events.subscribe<Event::MouseWheel>([&ctx](const Event::MouseWheel& event) {
        ctx.input.onScroll(event.delta);
        return false;
    });
    ctx.events.subscribe<Event::WindowResize>([&ctx](const Event::WindowResize& event) {
        if (event.width == 0 || event.height == 0) return false;
        ctx.width = event.width;
        ctx.height = event.height;

        // Windows pauses the main loop while resizing, so draw here
        if (ctx.running) render(ctx);
        return false;
    });
    ctx.events.subscribe<Event::Quit>([&ctx](const Event::Quit&) {
        ctx.running = false;
        return true;
    });
}

void Editor::run(EditorContext& ctx) {
    ctx.running = true;

    while (ctx.running) {
        ctx.input.beginFrame();
        Platform::pollEvents();
        if (!ctx.running) break;

        // The UI decides first whether it owns the mouse and keyboard this frame
        ctx.ui.beginFrame(makeUIInput(ctx.input), !ctx.contexts.isActive(InputContext_Console));
        ctx.actions.setMouseBlocked(ctx.ui.wantsMouse());
        ctx.actions.setKeyboardBlocked(ctx.ui.wantsKeyboard());

        update(ctx);
        render(ctx);
        ctx.window->setCursor(toCursorShape(ctx.ui.cursor()));
    }
}

void Editor::render(EditorContext& ctx) {
    ctx.frameTimer.tick(std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count());

    glViewport(0, 0, static_cast<GLsizei>(ctx.width), static_cast<GLsizei>(ctx.height));
    glClearColor(0.10f, 0.10f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ctx.drawList.clear();
    drawInterface(ctx);
    ctx.uiRenderer.draw(ctx.drawList, ctx.fontTextures, ctx.width, ctx.height);

    ctx.graphics.present();
}

void Editor::updateWindowTitle(EditorContext& ctx) {
    ctx.window->setTitle(ctx.project ? ctx.project->name + " - " + APP_NAME : std::string(APP_NAME));
}
