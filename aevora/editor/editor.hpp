#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <optional>

#include "core/console/command_system.hpp"
#include "core/console/console.hpp"
#include "core/events/event_dispatcher.hpp"
#include "core/font/font_library.hpp"
#include "core/input/action_map.hpp"
#include "core/input/input_state.hpp"
#include "core/time/frame_timer.hpp"
#include "editor/input/actions.hpp"
#include "editor/input/contexts.hpp"
#include "editor/input/default_keybinds.hpp"
#include "engine/jobs/job_system.hpp"
#include "engine/project/project.hpp"
#include "gfx/opengl/opengl_context.hpp"
#include "gfx/opengl/opengl_font.hpp"
#include "gfx/opengl/opengl_ui_renderer.hpp"
#include "platform/window/window.hpp"
#include "ui/console_view.hpp"
#include "ui/ui_context.hpp"

enum class Workspace : u8 {
    World,
    Scene,
    Data,
    Count
};

const char* workspaceName(Workspace workspace);

// A file dialog asked for this frame; it opens at the start of the next one, outside input handling and drawing
enum class Request : u8 {
    None,
    NewProject,
    OpenProject
};

struct EditorContext {
    EventDispatcher events;
    InputState input;
    ActionMap actions;
    ContextManager contexts = makeInputContexts();
    Console console;
    CommandSystem commands;

    std::unique_ptr<Window> window;
    u32 width = 0;
    u32 height = 0;

    OpenGLContext graphics;
    FontLibrary fonts;
    std::array<OpenGLFont, static_cast<u32>(FontId::Count)> fontTextures;
    OpenGLUIRenderer uiRenderer;
    UIDrawList drawList;
    UIContext ui;
    ConsoleViewState consoleView;
    FrameTimer frameTimer;

    JobSystem jobs;

    std::optional<Project> project;
    Workspace workspace = Workspace::World;
    Request request = Request::None;
    std::filesystem::path projectsFolder;   // where the dialogs start

    bool running = false;
};

namespace Editor {
    bool initialize(EditorContext& ctx);
    void run(EditorContext& ctx);
    void shutdown(EditorContext& ctx);

    void registerEvents(EditorContext& ctx);
    void registerActions(EditorContext& ctx);
    void registerCommands(EditorContext& ctx);

    // Runs the frame's keys and any file dialog asked for last frame
    void update(EditorContext& ctx);
    void render(EditorContext& ctx);
    void drawInterface(EditorContext& ctx);

    bool newProject(EditorContext& ctx, const std::filesystem::path& folder);
    bool openProject(EditorContext& ctx, const std::filesystem::path& file);
    void updateWindowTitle(EditorContext& ctx);
}
