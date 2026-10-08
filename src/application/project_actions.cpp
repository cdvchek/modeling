#include "application/project_actions.hpp"
#include "application/application.hpp"
#include "application/editing_actions.hpp"
#include "platform/platform.hpp"

#include <chrono>

namespace {
    constexpr const char* APP_NAME = "Valuma Studio";
    constexpr const char* FILE_TYPE = "Valuma Studio project";
    constexpr const char* UNTITLED = "Untitled";
    constexpr const char* PROJECTS_FOLDER = "Valuma Studio";
    constexpr u32 TOOL_CONTEXTS = InputContext_Grab | InputContext_Scale | InputContext_Rotate | InputContext_Bevel | InputContext_Inset;

    std::string displayName(const std::filesystem::path& path) {
        const std::u8string name = path.filename().u8string();
        return std::string(name.begin(), name.end());
    }

    std::string projectName(const AppContext& ctx) {
        return ctx.project.path.empty() ? UNTITLED : displayName(ctx.project.path);
    }

    void* nativeWindow(AppContext& ctx) {
        return ctx.windows.empty() ? nullptr : ctx.windows[0]->getNativeHandle();
    }

    // A dialog takes the key-ups for keys held when it opened (like Ctrl), so forget them
    void afterDialog(AppContext& ctx) {
        ctx.systems.input.releaseAll();
    }

    std::string millisecondsSince(std::chrono::steady_clock::time_point start) {
        const auto elapsed = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - start).count();
        return std::to_string(static_cast<i64>(elapsed + 0.5)) + " ms";
    }

    ProjectFile::View captureView(const AppContext& ctx) {
        ProjectFile::View view;
        view.selectionMode = ctx.systems.input_ctx.getSelectionContext();
        view.lastEditMode = ctx.lastEditMode;
        view.debug = ctx.systems.input_ctx.isActive(InputContext_Debug);
        view.headlightEnabled = ctx.viewport.headlight.enabled;
        view.headlightColor = ctx.viewport.headlight.color;
        view.headlightStrength = ctx.viewport.headlight.strength;
        if (ctx.renderer) view.backFaceTint = ctx.renderer->getBackFaceTint();
        view.showPanel = ctx.viewport.showPanel;
        view.panelRect = ctx.viewport.panel.rect;
        view.panelTab = ctx.viewport.panel.activeTab;
        return view;
    }

    // withPanel: also move the panel; a new project leaves it where it is
    void applyView(AppContext& ctx, const ProjectFile::View& view, bool withPanel) {
        ctx.viewport.headlight.enabled = view.headlightEnabled;
        ctx.viewport.headlight.color = view.headlightColor;
        ctx.viewport.headlight.strength = view.headlightStrength;
        if (ctx.renderer) ctx.renderer->setBackFaceTint(view.backFaceTint);

        if (view.debug) ctx.systems.input_ctx.addContext(InputContext_Debug);
        else ctx.systems.input_ctx.removeContext(InputContext_Debug);

        if (withPanel) {
            ctx.viewport.showPanel = view.showPanel;
            ctx.viewport.panel.rect = view.panelRect;
            ctx.viewport.panel.activeTab = view.panelTab;
            ctx.viewport.panel.scroll = 0.0f;
        }

        // Mode last: it selects the active object in object mode, and sets lastEditMode for edit modes
        setSelectionMode(ctx, view.selectionMode);
        ctx.lastEditMode = view.lastEditMode;
        ctx.radialMenu.open = false;
    }

    void markSaved(AppContext& ctx) {
        ctx.project.savedState = ctx.history.stateId();
    }
}

bool canUseProjectFiles(const AppContext& ctx) {
    return !(ctx.systems.input_ctx.getContext() & TOOL_CONTEXTS);
}

bool hasUnsavedChanges(const AppContext& ctx) {
    return ctx.history.stateId() != ctx.project.savedState;
}

std::filesystem::path withProjectExtension(std::filesystem::path path) {
    if (!path.has_extension()) path += ProjectFile::EXTENSION;
    return path;
}

std::filesystem::path projectsFolder() {
    const std::filesystem::path documents = Platform::documentsFolder();
    std::filesystem::path folder = documents.empty() ? std::filesystem::current_path() : documents / PROJECTS_FOLDER;

    std::error_code code;
    std::filesystem::create_directories(folder, code);
    return folder;
}

std::filesystem::path resolveProjectPath(const std::filesystem::path& path) {
    const std::filesystem::path named = withProjectExtension(path);
    return named.is_absolute() ? named : projectsFolder() / named;
}

bool saveProjectTo(AppContext& ctx, const std::filesystem::path& path) {
    if (!canUseProjectFiles(ctx)) {
        ctx.systems.console.printError("Can't save while a tool is running; confirm or cancel it first");
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    const std::filesystem::path target = std::filesystem::absolute(path);

    std::string error;
    if (!ProjectFile::save(target, ctx.scene, captureView(ctx), error)) {
        ctx.systems.console.printError("Couldn't save: " + error);
        return false;
    }

    ctx.project.path = target;
    markSaved(ctx);
    ctx.systems.console.print("Saved " + displayName(target) + " in " + millisecondsSince(start));
    return true;
}

bool saveProject(AppContext& ctx) {
    if (ctx.project.path.empty()) return saveProjectAs(ctx);
    return saveProjectTo(ctx, ctx.project.path);
}

bool saveProjectAs(AppContext& ctx) {
    if (!canUseProjectFiles(ctx)) {
        ctx.systems.console.printError("Can't save while a tool is running; confirm or cancel it first");
        return false;
    }

    // A saved project offers its own folder and name; a new one starts in the projects folder
    const bool saved = !ctx.project.path.empty();
    const std::filesystem::path folder = saved ? ctx.project.path.parent_path() : projectsFolder();
    const std::filesystem::path name = saved ? ctx.project.path.filename() : std::filesystem::path(std::string(UNTITLED) + ProjectFile::EXTENSION);
    const std::filesystem::path path = Platform::chooseSaveFile(nativeWindow(ctx), FILE_TYPE, ProjectFile::EXTENSION, folder, name);
    afterDialog(ctx);

    if (path.empty()) return false;
    return saveProjectTo(ctx, path);
}

bool openProjectFrom(AppContext& ctx, const std::filesystem::path& path) {
    if (!canUseProjectFiles(ctx)) {
        ctx.systems.console.printError("Can't open a project while a tool is running; confirm or cancel it first");
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    const std::filesystem::path target = std::filesystem::absolute(path);

    // Read into a separate scene, so a bad file leaves the current one alone
    Scene scene;
    ProjectFile::View view = captureView(ctx);
    std::string error;

    if (!ProjectFile::load(target, scene, view, error)) {
        ctx.systems.console.printError("Couldn't open " + displayName(target) + ": " + error);
        return false;
    }

    ctx.scene = std::move(scene);
    ctx.history.clear();
    applyView(ctx, view, true);

    ctx.project.path = target;
    markSaved(ctx);
    ctx.systems.console.print("Opened " + displayName(target) + " (" + std::to_string(ctx.scene.objects.count()) + " objects, "
        + std::to_string(ctx.scene.lights.count()) + " lights) in " + millisecondsSince(start));
    return true;
}

void openProject(AppContext& ctx) {
    if (!canUseProjectFiles(ctx) || !confirmDiscardChanges(ctx)) return;

    const std::filesystem::path folder = ctx.project.path.empty() ? projectsFolder() : ctx.project.path.parent_path();
    const std::filesystem::path path = Platform::chooseOpenFile(nativeWindow(ctx), FILE_TYPE, ProjectFile::EXTENSION, folder);
    afterDialog(ctx);

    if (!path.empty()) openProjectFrom(ctx, path);
}

void newProject(AppContext& ctx) {
    if (!canUseProjectFiles(ctx) || !confirmDiscardChanges(ctx)) return;

    ctx.scene = Scene();
    Application::initializeCamera(ctx);
    Application::loadTestScene(ctx);
    ctx.history.clear();
    applyView(ctx, ctx.project.startingView, false);

    ctx.project.path.clear();
    markSaved(ctx);
    ctx.systems.console.print("New project");
}

bool confirmDiscardChanges(AppContext& ctx) {
    if (!hasUnsavedChanges(ctx)) return true;

    const Platform::SaveChoice choice = Platform::askToSaveChanges(nativeWindow(ctx), APP_NAME, projectName(ctx));
    afterDialog(ctx);

    switch (choice) {
        case Platform::SaveChoice::Save: return saveProject(ctx);
        case Platform::SaveChoice::DontSave: return true;
        default: return false;
    }
}

void updateWindowTitle(AppContext& ctx) {
    const std::string title = projectName(ctx) + (hasUnsavedChanges(ctx) ? "*" : "") + " - " + APP_NAME;
    if (title == ctx.project.shownTitle || ctx.windows.empty()) return;

    ctx.windows[0]->setTitle(title);
    ctx.project.shownTitle = title;
}

void initializeProject(AppContext& ctx) {
    ctx.project.startingView = captureView(ctx);
    markSaved(ctx);
    updateWindowTitle(ctx);
}
