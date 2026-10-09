#pragma once

#include "application/app_context.hpp"

// One-shot editing actions, run from keys through the action registry
// Vertex, edge, face, or object mode; clears the selection (object mode then selects the active object)
void setSelectionMode(AppContext& ctx, u32 mode);
// Tab: object mode, or back to the last edit mode
void toggleObjectMode(AppContext& ctx);
void toggleAxis(AppContext& ctx, u32 axis);

void undo(AppContext& ctx);
void redo(AppContext& ctx);

bool canGrab(const AppContext& ctx);
void startGrab(AppContext& ctx);
bool canScale(const AppContext& ctx);
void startScale(AppContext& ctx);
bool canRotate(const AppContext& ctx);
void startRotate(AppContext& ctx);

// Exactly one vertex, edge, or face in the matching mode
bool canBevel(const AppContext& ctx);

// Extrude and inset (beginInset) work on every selected face region
bool canExtrude(const AppContext& ctx);
void extrudeSelection(AppContext& ctx);

bool canConnectVertices(const AppContext& ctx);
void connectVertices(AppContext& ctx);
bool canFillFaceLoop(const AppContext& ctx);
void fillFaceLoop(AppContext& ctx);

bool canMergeVertices(const AppContext& ctx);
// mergeType: 0 = at the center, 1 = at the first vertex, 2 = at the last
void mergeVertices(AppContext& ctx, u8 mergeType = 0);
bool canDissolve(const AppContext& ctx);
void dissolveSelection(AppContext& ctx);

// Ctrl+P (object mode): the selected objects become children of the active object, staying where they are.
// One that would become its own parent's parent is skipped, with a console error.
bool canParentToActive(const AppContext& ctx);
void parentToActive(AppContext& ctx);
// Alt+P (object mode): the selected objects lose their parents, staying where they are
bool canClearParents(const AppContext& ctx);
void clearParents(AppContext& ctx);

// Removes every selected object as one undo step
void deleteSelectedObjects(AppContext& ctx);

bool canDelete(const AppContext& ctx);
void deleteSelection(AppContext& ctx);

// Axis locks while grab, scale, or rotate runs
bool canChangeAxis(const AppContext& ctx);
void clearAxes(AppContext& ctx);

// View settings: not part of the scene, so not undoable
void togglePanel(AppContext& ctx);
void toggleHeadlight(AppContext& ctx);
void toggleDebugView(AppContext& ctx);
// Material view (as in the game) or clay view (everything the Default gray, back faces tinted); not undoable
void toggleMaterials(AppContext& ctx);
// The UV checker grid over every face; not undoable
void toggleUVChecker(AppContext& ctx);

bool canSetLightType(const AppContext& ctx, LightType type);
void setLightType(AppContext& ctx, LightType type);
bool canToggleLights(const AppContext& ctx);
void toggleLights(AppContext& ctx);
