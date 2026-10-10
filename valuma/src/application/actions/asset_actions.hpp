#pragma once

#include <filesystem>
#include "application/app_context.hpp"

// Exporting objects as .vlmobj assets (the Export window) and importing them as new objects

// Where the Export window starts: the folder last exported to, or an Exports folder next to the project file
// (Documents\Valuma Studio\Exports for a project that hasn't been saved)
std::filesystem::path exportFolder(const AppContext& ctx);

// Ctrl+E: opens the Export window with the selected objects checked (in an edit mode, the object being edited)
void openExportWindow(AppContext& ctx);
void drawExportWindow(AppContext& ctx, const Rect& viewport);
// Browse and Export clicks from the last draw
void updateExportWindow(AppContext& ctx);
// Enter: exports if anything will be written
void confirmExportWindow(AppContext& ctx);

// Ctrl+I (and Import… in the Objects tab): picks .vlmobj files and adds each as a new object
void importAssets(AppContext& ctx);
// Adds one asset as a new object, placed like a new preset and named after the file (made unique); one undo step
bool importAssetFrom(AppContext& ctx, const std::filesystem::path& path);
