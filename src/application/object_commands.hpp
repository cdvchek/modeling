#pragma once

#include <string>
#include <vector>
#include "application/app_context.hpp"

struct ObjectPreset {
    const char* command;       // name used by "object add"
    const char* displayName;   // shown in the UI and used for new object names
    PresetMesh preset;
};

const std::vector<ObjectPreset>& objectPresets();

// Adds an object (spaced out along X), makes it the one being edited, as one undo step
ObjectHandle addObject(AppContext& ctx, PresetMesh preset, const std::string& name);

// Removes an object as one undo step; if it was being edited, the first remaining object takes over
void removeObject(AppContext& ctx, ObjectHandle handle);

void runObjectCommand(AppContext& ctx, const CommandArgs& args);
