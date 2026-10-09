#pragma once

#include <vector>
#include "core/math/vec2.hpp"
#include "scene/objects/object_collection.hpp"

struct AppContext;

// Grab, scale, and rotate in the UV editor: the selected corners follow the mouse until a click or Enter keeps the
// change (one undo step) or Escape or a right click puts it back
enum class UVToolKind : u8 {
    None,
    Grab,
    Scale,
    Rotate
};

enum class UVAxis : u8 {
    Free,
    X,      // u only
    Y       // v only
};

struct UVToolState {
    UVToolKind kind = UVToolKind::None;
    UVAxis axis = UVAxis::Free;
    ObjectHandle object = INVALID_OBJECT;
    std::vector<Vec2> startUVs;     // every corner's UV when the tool started, in getCornerUVs order
    std::vector<u32> moving;        // which of them the tool moves
    Vec2 pivot;                     // UV space: the middle of the moving corners' bounds
    Vec2 startMouse;                // window pixels
    f32 angle = 0.0f;               // rotate: how far the mouse has swept around the pivot
    f32 lastAngle = 0.0f;

    bool active() const { return kind != UVToolKind::None; }
};

// The corners a tool moves: the selected faces' corners in face and island mode (an island moves on its own),
// otherwise every corner at a selected vertex. Indices into the object's getCornerUVs.
std::vector<u32> uvToolCorners(const AppContext& ctx);

// There's something to move: the object being UV-edited has selected corners
bool canStartUVTool(const AppContext& ctx);
void startUVTool(AppContext& ctx, UVToolKind kind);

// Each frame while a tool runs: X and Y toggle the axis lock, a click or Enter keeps the change, Escape or a right
// click puts it back, and the corners follow the mouse. It takes all input while it runs.
void updateUVTool(AppContext& ctx);

// For the status bar: "Grab", "Scale", or "Rotate", and the axis lock
const char* uvToolName(UVToolKind kind);
