#pragma once

#include "application/app_context.hpp"

// shading [flat | smooth | auto [<degrees>]] | shading mark <hard | smooth | clear>
void runShadingCommand(AppContext& ctx, const CommandArgs& args);

// "flat", "smooth", "auto"
const char* shadingName(ShadingMode mode);

// The objects shading acts on: the selected objects in object mode, otherwise the active object; none while a tool runs
std::vector<ObjectHandle> shadingTargets(const AppContext& ctx);
bool canSetShading(const AppContext& ctx);
// Gives each target the mode, as one undo step
void setShading(AppContext& ctx, ShadingMode mode);

// Edge mode with edges selected on the active object, and no tool running
bool canMarkEdges(const AppContext& ctx);
// Marks the selected edges (EdgeMark::None clears them), as one undo step
void markSelectedEdges(AppContext& ctx, EdgeMark mark);
