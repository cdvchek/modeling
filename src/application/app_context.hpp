#pragma once

#include <vector>
#include <memory>

#include "application/app_systems.hpp"
#include "application/tools/width_tool.hpp"
#include "application/viewport/viewport_settings.hpp"
#include "platform/window/window.hpp"
#include "renderer/renderer.hpp"
#include "renderer/debug_renderer.hpp"
#include "scene/scene.hpp"
#include "scene/history.hpp"
#include "core/font/font_library.hpp"
#include "ui/ui_draw_list.hpp"
#include "ui/ui_context.hpp"
#include "application/viewport/object_meshes.hpp"
#include "application/viewport/reference_images.hpp"
#include "application/viewport/material_view.hpp"
#include "application/ui/stats_overlay.hpp"
#include "application/ui/radial_menu_state.hpp"
#include "application/ui/console_view_state.hpp"
#include "application/tools/transform_tool.hpp"
#include "application/project_state.hpp"
#include "application/workspace.hpp"
#include "application/uv/uv_tools.hpp"
#include "application/ui/modal_state.hpp"
#include "core/time/frame_timer.hpp"

struct AppContext {
    Systems systems;
    std::unique_ptr<IRenderer> renderer;
    DebugRenderer debug_renderer;
    std::vector<std::unique_ptr<Window>> windows;
    Scene scene;
    WidthTool widthTool;
    TransformTool transformTool;
    OriginEdit originEdit;
    History history;
    ViewportSettings viewport;
    FontLibrary fonts;
    UIDrawList uiDrawList;
    UIContext ui;
    ObjectMeshCache objectMeshes;
    PictureTextureCache pictureTextures;
    LayerTextureCache layerTextures;
    MaterialPreviewCache materialPreviews;
    FrameTimer frameTimer;
    FrameStats frameStats;

    RadialMenuState radialMenu;
    ConsoleViewState consoleView;
    ProjectState project;
    ModalState modal;
    WorkspaceState workspace;
    UVToolState uvTool;         // a grab, scale, or rotate running in the UV editor
    Stroke paintStroke;         // a brush stroke being drawn in the Paint workspace
    ModelStroke modelStroke;    // and what it keeps between frames when it's on the model

    // Import… was picked in the Objects tab; the file dialog opens in checkActions, not while drawing
    bool importRequested = false;
    // Likewise for + in the References tab
    bool referenceRequested = false;
    // Where the image dialog last picked from (this session only)
    std::filesystem::path referenceFolder;
    // Load PNG… was picked for a texture (and the material to put it on, if any); opened in checkActions
    struct TextureRequest {
        bool open = false;
        MaterialHandle material = INVALID_MATERIAL;
    } textureRequest;
    // Where the texture dialog last picked from (this session only)
    std::filesystem::path textureFolder;

    // Where Tab returns to from object mode
    u32 lastEditMode = InputContext_SelectionVertex;

    bool is_running = false;
};