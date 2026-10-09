#pragma once

#include "core/math/mat4.hpp"
#include "core/math/vec2.hpp"
#include "ui/ui_types.hpp"

struct AppContext;

// A screen layout for one kind of work; the scene is the same in every workspace
enum class Workspace : u8 {
    Model,
    UV
};

const char* workspaceName(Workspace workspace);

struct WorkspaceState {
    Workspace current = Workspace::Model;
    // UV workspace: where the divider between the 3D view and the UV editor sits, as a fraction of the width
    f32 uvSplit = 0.5f;
};

// Where everything sits in the window this frame, in window pixels (origin top left, y down)
struct ScreenLayout {
    Rect window;
    Rect topBar;        // the workspace tabs
    Rect content;       // between the top bar and the status bar
    Rect scene;         // the 3D view
    Rect divider;       // UV workspace: between the 3D view and the UV editor; empty otherwise
    Rect uvEditor;      // UV workspace: right of the divider; empty otherwise
};

inline constexpr f32 TOP_BAR_HEIGHT = 30.0f;
inline constexpr f32 DIVIDER_WIDTH = 6.0f;
// Neither side of the UV workspace gets narrower than this (unless the window itself is too narrow)
inline constexpr f32 MIN_SPLIT_WIDTH = 200.0f;

ScreenLayout screenLayout(const AppContext& ctx);

// The 3D view's rect, and the camera's view-projection for it (its aspect, not the window's)
Rect sceneView(const AppContext& ctx);
Mat4 sceneViewProjection(const AppContext& ctx);

// World point to window pixels inside view; false if it's behind the camera
bool projectToView(const Mat4& viewProjection, const Vec3& point, const Rect& view, Vec2& out);

// Switches workspace; refused (false) while a tool runs. Not undoable: it's how you look at the scene, not the scene.
bool setWorkspace(AppContext& ctx, Workspace workspace);
