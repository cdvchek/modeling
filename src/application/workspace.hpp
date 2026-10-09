#pragma once

#include "core/input/actions.hpp"
#include "core/math/mat4.hpp"
#include "core/math/vec2.hpp"
#include "ui/ui_types.hpp"
#include "scene/objects/object_collection.hpp"
#include "scene/textures/texture.hpp"

#include <string>

struct AppContext;

// A screen layout for one kind of work; the scene is the same in every workspace
enum class Workspace : u8 {
    Model,
    UV
};

const char* workspaceName(Workspace workspace);

// What the UV editor shows behind the UVs
enum class UVBackground : u8 {
    Material,   // the base color map of the selected faces' material (a checker when it has none)
    Checker,
    Texture     // a chosen texture (uvTexture)
};

struct WorkspaceState {
    Workspace current = Workspace::Model;
    // UV workspace: where the divider between the 3D view and the UV editor sits, as a fraction of the width
    f32 uvSplit = 0.5f;

    // The UV editor's view: the UV point at its middle, and pixels per UV unit (0: frame everything when next drawn)
    Vec2 uvCenter { 0.5f, 0.5f };
    f32 uvZoom = 0.0f;
    bool uvGrid = true;
    UVBackground uvBackground = UVBackground::Material;
    TextureHandle uvTexture = INVALID_TEXTURE;
    bool uvPanning = false;     // a drag that started over the editor is moving the view
    // Island mode (UV workspace): face mode, but a click takes every face connected to it on the texture
    bool uvIslands = false;
    // The gap left around each island when packing, in percent of the texture's width
    f32 uvMargin = 1.0f;
};

// Where everything sits in the window this frame, in window pixels (origin top left, y down)
struct ScreenLayout {
    Rect window;
    Rect topBar;        // the workspace tabs
    Rect content;       // between the top bar and the status bar
    Rect header;        // UV workspace: above both views (the object being UV-edited); empty otherwise
    Rect scene;         // the 3D view
    Rect divider;       // UV workspace: between the 3D view and the UV editor; empty otherwise
    Rect uvEditor;      // UV workspace: right of the divider; empty otherwise
    Rect uvTools;       // UV workspace: the tools column at the right edge; empty otherwise
};

inline constexpr f32 TOP_BAR_HEIGHT = 30.0f;
inline constexpr f32 UV_HEADER_HEIGHT = 34.0f;
inline constexpr f32 DIVIDER_WIDTH = 6.0f;
// Neither side of the UV workspace gets narrower than this (unless the window itself is too narrow)
inline constexpr f32 MIN_SPLIT_WIDTH = 200.0f;
inline constexpr f32 UV_TOOLS_WIDTH = 210.0f;

ScreenLayout screenLayout(const AppContext& ctx);

// The 3D view's rect, and the camera's view-projection for it (its aspect, not the window's)
Rect sceneView(const AppContext& ctx);
Mat4 sceneViewProjection(const AppContext& ctx);

// World point to window pixels inside view; false if it's behind the camera
bool projectToView(const Mat4& viewProjection, const Vec3& point, const Rect& view, Vec2& out);

// Switches workspace; refused (false) while a tool runs. Not undoable: it's how you look at the scene, not the scene.
// Entering UV makes sure there's an object to work on (the first one when none is being edited), leaves object mode
// for the last edit mode, and drops selected lights, reference images, and origins, which UV has no use for.
bool setWorkspace(AppContext& ctx, Workspace workspace);

// Whether the action belongs to the workspace: UV has the camera, selecting, the edit modes, and app-wide actions
// (undo and redo, files, the console, view toggles); Model has everything
bool actionAllowed(Workspace workspace, Action action);

// Console commands that change meshes, objects, lights, materials, textures, or reference images: Model only
bool isModelCommand(const std::string& name);

// The object the UV workspace works on: the one being edited; invalid when there are no objects
ObjectHandle uvObject(const AppContext& ctx);
