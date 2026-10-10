#pragma once

#include "core/input/actions.hpp"
#include "core/math/mat4.hpp"
#include "core/math/vec2.hpp"
#include "ui/ui_types.hpp"
#include "scene/objects/object_collection.hpp"
#include "scene/textures/texture.hpp"
#include "scene/textures/brush.hpp"

#include <memory>
#include <string>
#include <vector>

struct AppContext;

// A screen layout for one kind of work; the scene is the same in every workspace
enum class Workspace : u8 {
    Model,
    UV,
    Paint
};

const char* workspaceName(Workspace workspace);

// What the UV editor shows behind the UVs
enum class UVBackground : u8 {
    Material,   // the base color map of the selected faces' material (a checker when it has none)
    Checker,
    Texture     // a chosen texture (uvTexture)
};

// A flat view of a texture (the UV editor, the paint canvas): the UV point at its middle and pixels per UV unit
// (0: frame everything when next drawn)
struct UVView {
    Vec2 center { 0.5f, 0.5f };
    f32 zoom = 0.0f;
    bool panning = false;       // a drag that started over the view is moving it
};

struct WorkspaceState {
    Workspace current = Workspace::Model;
    // UV workspace: where the divider between the 3D view and the UV editor sits, as a fraction of the width
    f32 uvSplit = 0.5f;

    UVView uvView;              // the UV editor's
    bool uvGrid = true;
    UVBackground uvBackground = UVBackground::Material;
    TextureHandle uvTexture = INVALID_TEXTURE;
    // Island mode (UV workspace): face mode, but a click takes every face connected to it on the texture
    bool uvIslands = false;
    // The gap left around each island when packing, in percent of the texture's width
    f32 uvMargin = 1.0f;

    // Paint workspace: the flat texture (2D) instead of the model (3D), the 2D view, and the texture being painted
    // (kept to one the object uses: see activePaintTexture)
    bool paint2D = false;
    UVView paintView;
    TextureHandle paintTexture = INVALID_TEXTURE;
    // The brush (or eraser) and its settings, saved with the project's view
    Brush brush;
};

// What a stroke on the model keeps between frames
struct ModelStroke {
    // An island the stroke has reached: its paintable faces in handle order, and the mask that keeps dabs to them
    struct Island {
        std::vector<FaceHandle> faces;
        std::shared_ptr<const PaintMask> mask;
    };

    bool onModel = false;       // the stroke runs in the 3D view
    bool moved = false;         // lastMouse is set
    Vec2 lastMouse;
    bool touching = false;      // the last sample landed on a paintable face (lastPoint, island)
    Vec3 lastPoint;
    std::size_t island = 0;
    std::vector<Island> islands;
};

// Where everything sits in the window this frame, in window pixels (origin top left, y down)
struct ScreenLayout {
    Rect window;
    Rect topBar;        // the workspace tabs
    Rect content;       // between the top bar and the status bar
    Rect header;        // UV and Paint: across the top (the object being worked on); empty otherwise
    Rect scene;         // the 3D view (Paint: the viewport, also in 2D, when nothing 3D draws there)
    Rect divider;       // UV workspace: between the 3D view and the UV editor; empty otherwise
    Rect uvEditor;      // UV workspace: right of the divider; empty otherwise
    Rect uvTools;       // UV workspace: the tools column at the right edge; empty otherwise
    Rect paintCanvas;   // Paint in 2D: the flat texture, where the viewport is; empty otherwise
    Rect paintTools;    // Paint: the tools column at the right edge; empty otherwise
};

inline constexpr f32 TOP_BAR_HEIGHT = 30.0f;
inline constexpr f32 WORKSPACE_HEADER_HEIGHT = 34.0f;
inline constexpr f32 DIVIDER_WIDTH = 6.0f;
// Neither side of the UV workspace gets narrower than this (unless the window itself is too narrow)
inline constexpr f32 MIN_SPLIT_WIDTH = 200.0f;
inline constexpr f32 TOOLS_WIDTH = 210.0f;
// Paint's column holds labeled fields (a layer's name and opacity), so it's wider
inline constexpr f32 PAINT_TOOLS_WIDTH = 290.0f;

ScreenLayout screenLayout(const AppContext& ctx);

// The 3D view's rect, and the camera's view-projection for it (its aspect, not the window's)
Rect sceneView(const AppContext& ctx);
Mat4 sceneViewProjection(const AppContext& ctx);

// World point to window pixels inside view; false if it's behind the camera
bool projectToView(const Mat4& viewProjection, const Vec3& point, const Rect& view, Vec2& out);

// Switches workspace; refused (false) while a tool runs. Not undoable: it's how you look at the scene, not the scene.
// Entering UV or Paint makes sure there's an object to work on (the first one when none is being edited), leaves
// object mode for the last edit mode, and drops selected lights, reference images, and origins, which they have no
// use for.
bool setWorkspace(AppContext& ctx, Workspace workspace);

// Whether the action belongs to the workspace: UV has the camera, selecting, the edit modes, and app-wide actions
// (undo and redo, files, the console, view toggles); Paint has the camera, framing, Tab between 3D and 2D, Alt+click
// to pick a texture, the brush keys, and the app-wide actions; Model has everything else
bool actionAllowed(Workspace workspace, Action action);

// Console commands that change meshes, objects, lights, materials, textures, or reference images: Model only
bool isModelCommand(const std::string& name);

// The object the UV and Paint workspaces work on: the one being edited; invalid when there are no objects
ObjectHandle uvObject(const AppContext& ctx);
