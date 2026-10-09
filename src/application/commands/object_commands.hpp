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
// Same placement for an object built elsewhere (an imported asset); its rotation and scale are kept
ObjectHandle addObject(AppContext& ctx, Object object);
// The same placement and selection without its own undo step, for callers adding several objects as one step
ObjectHandle placeNewObject(AppContext& ctx, Object object);

// Gives child a new parent (INVALID_OBJECT: none) as one undo step, keeping it where it is; prints what happened,
// or an error if the parent is the child or one of its children
bool setObjectParent(AppContext& ctx, ObjectHandle child, ObjectHandle parent);

// Removes an object as one undo step; if it was being edited, the first remaining object takes over
void removeObject(AppContext& ctx, ObjectHandle handle);

void runObjectCommand(AppContext& ctx, const CommandArgs& args);
