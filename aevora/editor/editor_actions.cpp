#include "editor/editor.hpp"

#include <algorithm>
#include <iostream>

#include "core/console/console_input.hpp"
#include "platform/platform.hpp"

namespace {
    constexpr const char* FILE_TYPE = "Aevora Engine project";

    std::string pathText(const std::filesystem::path& path) {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    void toggleConsole(EditorContext& ctx) {
        ctx.contexts.toggleContext(InputContext_Console);

        // Closing collapses what's there; anything printed after this shows open next time
        if (!ctx.contexts.isActive(InputContext_Console)) ctx.console.collapseEntries();
        else ctx.consoleView.followNewest = true;
    }

    // A dialog takes the key-ups for keys held when it opened, so they're forgotten after it
    void runRequest(EditorContext& ctx) {
        const Request request = ctx.request;
        ctx.request = Request::None;
        if (request == Request::None) return;

        void* window = ctx.window->getNativeHandle();
        if (request == Request::NewProject) {
            const std::filesystem::path folder = Platform::chooseFolder(window, ctx.projectsFolder);
            if (!folder.empty()) Editor::newProject(ctx, folder);
        } else {
            const std::filesystem::path file = Platform::chooseOpenFile(window, FILE_TYPE, ProjectFile::EXTENSION, ctx.projectsFolder);
            if (!file.empty()) Editor::openProject(ctx, file);
        }
        ctx.input.releaseAll();
    }
}

bool Editor::newProject(EditorContext& ctx, const std::filesystem::path& folder) {
    Project project;
    std::string error;
    if (!ProjectFile::create(folder, project, error)) {
        ctx.console.printError("New project: " + error);
        return false;
    }

    ctx.project = project;
    ctx.projectsFolder = project.folder().parent_path();
    ctx.workspace = Workspace::World;
    updateWindowTitle(ctx);
    ctx.console.print("Made " + pathText(project.file));
    return true;
}

bool Editor::openProject(EditorContext& ctx, const std::filesystem::path& file) {
    Project project;
    std::string error;
    if (!ProjectFile::load(file, project, error)) {
        ctx.console.printError("Open project: " + error);
        return false;
    }

    ctx.project = project;
    ctx.projectsFolder = project.folder().parent_path();
    ctx.workspace = Workspace::World;
    updateWindowTitle(ctx);
    ctx.console.print("Opened " + pathText(project.file));
    return true;
}

void Editor::registerActions(EditorContext& ctx) {
    ActionMap& actions = ctx.actions;

    // The console owns the input while it's open; a text field never stops quitting
    actions.setOwnerContexts({ InputContext_Console });
    actions.setUnblockable(Action::Quit);

    actions.subscribe(Action::Quit,                DefaultKeybinds::Quit,                InputContext_Global);
    actions.subscribe(Action::ToggleConsole,       DefaultKeybinds::ToggleConsole,       InputContext_Global | InputContext_Console);
    actions.subscribe(Action::EnterCommand,        DefaultKeybinds::EnterCommand,        InputContext_Console);
    actions.subscribe(Action::ConsoleBackspace,    DefaultKeybinds::ConsoleBackspace,    InputContext_Console);
    actions.subscribe(Action::ConsoleDelete,       DefaultKeybinds::ConsoleDelete,       InputContext_Console);
    actions.subscribe(Action::ConsoleCursorLeft,   DefaultKeybinds::ConsoleCursorLeft,   InputContext_Console);
    actions.subscribe(Action::ConsoleCursorRight,  DefaultKeybinds::ConsoleCursorRight,  InputContext_Console);
    actions.subscribe(Action::ConsoleHistoryOlder, DefaultKeybinds::ConsoleHistoryOlder, InputContext_Console);
    actions.subscribe(Action::ConsoleHistoryNewer, DefaultKeybinds::ConsoleHistoryNewer, InputContext_Console);
    actions.subscribe(Action::NewProject,          DefaultKeybinds::NewProject,          InputContext_Editor);
    actions.subscribe(Action::OpenProject,         DefaultKeybinds::OpenProject,         InputContext_Editor);

    actions.setHandler(Action::Quit, { "Quit", {}, [&ctx] { ctx.running = false; } });
    actions.setHandler(Action::ToggleConsole, { "Console", {}, [&ctx] { toggleConsole(ctx); } });
    actions.setHandler(Action::NewProject, { "New project", {}, [&ctx] { ctx.request = Request::NewProject; } });
    actions.setHandler(Action::OpenProject, { "Open project", {}, [&ctx] { ctx.request = Request::OpenProject; } });
}

void Editor::registerCommands(EditorContext& ctx) {
    ctx.console.setEcho(true);

    ctx.commands.registerCommand("help", "Lists every command and what it does: help", [&ctx](const CommandArgs&) {
        const auto commands = ctx.commands.list();

        std::size_t width = 0;
        for (const auto& [name, description] : commands) width = std::max(width, name.size());
        for (const auto& [name, description] : commands) std::cout << name << std::string(width - name.size() + 2, ' ') << description << '\n';
    });

    ctx.commands.registerCommand("quit", "Closes the editor: quit", [&ctx](const CommandArgs&) { ctx.running = false; });

    ctx.commands.registerCommand("project", "Shows the open project, or makes or opens one: project [new | open]", [&ctx](const CommandArgs& args) {
        if (!args.empty() && args[0] == "new") ctx.request = Request::NewProject;
        else if (!args.empty() && args[0] == "open") ctx.request = Request::OpenProject;
        else if (!args.empty()) ctx.console.printError("project: expected new or open");
        else if (ctx.project) std::cout << ctx.project->name << '\n' << pathText(ctx.project->file) << '\n';
        else std::cout << "No project is open\n";
    });

    ctx.commands.registerCommand("jobs", "Shows the job system's workers and what they're doing: jobs", [&ctx](const CommandArgs&) {
        const JobStats stats = ctx.jobs.stats();
        std::cout << stats.workers << " workers, " << stats.running << " running\n"
                  << "Queued: " << stats.queued[0] << " high, " << stats.queued[1] << " normal, " << stats.queued[2] << " low\n"
                  << "Completed: " << stats.completed << "\n"
                  << "Waiting for the main thread: " << stats.mainTasks << "\n";
    });

    ctx.commands.registerCommand("workspace", "Shows or switches the workspace: workspace [world | scene | data]", [&ctx](const CommandArgs& args) {
        if (args.empty()) {
            std::cout << workspaceName(ctx.workspace) << '\n';
            return;
        }
        for (u32 i = 0; i < static_cast<u32>(Workspace::Count); ++i) {
            std::string name = workspaceName(static_cast<Workspace>(i));
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name == args[0]) {
                ctx.workspace = static_cast<Workspace>(i);
                return;
            }
        }
        ctx.console.printError("workspace: expected world, scene, or data");
    });
}

void Editor::update(EditorContext& ctx) {
    runRequest(ctx);
    ctx.jobs.runMainTasks();

    ctx.actions.dispatch(ctx.input, ctx.contexts);

    if (ctx.contexts.isActive(InputContext_Console)) {
        updateConsoleView(ctx.console, ctx.consoleView, ctx.input);

        const ConsoleActions keys {
            Action::EnterCommand, Action::ConsoleBackspace, Action::ConsoleDelete,
            Action::ConsoleCursorLeft, Action::ConsoleCursorRight, Action::ConsoleHistoryOlder, Action::ConsoleHistoryNewer
        };
        updateConsoleInput(ctx.console, ctx.commands, keys, ctx.actions, ctx.input, ctx.contexts.getContext());
    }
}
