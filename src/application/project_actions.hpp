#pragma once

#include <filesystem>
#include "application/app_context.hpp"

// Saving, opening, and starting projects (.vlm). Each reports to the console; errors also flash in the status bar.

// Not while a tool like grab or bevel is running; confirm or cancel it first
bool canUseProjectFiles(const AppContext& ctx);

// Something undoable happened since the last save, open, or new
bool hasUnsavedChanges(const AppContext& ctx);

// Ctrl+S: saves to the project's file, or asks where like Save As the first time
bool saveProject(AppContext& ctx);
// Ctrl+Shift+S: always asks where
bool saveProjectAs(AppContext& ctx);
bool saveProjectTo(AppContext& ctx, const std::filesystem::path& path);

// Ctrl+O: offers to save changes, then asks which file
void openProject(AppContext& ctx);
bool openProjectFrom(AppContext& ctx, const std::filesystem::path& path);

// Ctrl+N: offers to save changes, then starts over with the default scene
void newProject(AppContext& ctx);

// Asks to save unsaved changes; false if you cancelled (or the save failed), so whatever was next shouldn't happen
bool confirmDiscardChanges(AppContext& ctx);

// "name.vlm* - Valuma Studio"; the * shows unsaved changes
void updateWindowTitle(AppContext& ctx);

// Remembers the startup view and marks the scene as saved, so a fresh start has no unsaved changes
void initializeProject(AppContext& ctx);

// path with .vlm added when it has no extension
std::filesystem::path withProjectExtension(std::filesystem::path path);

// Documents\Valuma Studio (the working folder if Documents can't be found), created if it isn't there.
// The dialogs start here, and console paths that aren't absolute are taken from here.
std::filesystem::path projectsFolder();

// A console path: .vlm added when there's no extension, and a relative path placed in projectsFolder()
std::filesystem::path resolveProjectPath(const std::filesystem::path& path);
