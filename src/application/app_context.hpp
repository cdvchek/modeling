#pragma once

#include <vector>
#include <memory>

#include "application/app_systems.hpp"
#include "application/width_tool.hpp"
#include "application/viewport_settings.hpp"
#include "platform/window/window.hpp"
#include "renderer/renderer.hpp"
#include "renderer/debug_renderer.hpp"
#include "scene/scene.hpp"
#include "scene/history.hpp"
#include "core/font/font_library.hpp"
#include "ui/ui_draw_list.hpp"
#include "ui/ui_context.hpp"
#include "application/object_meshes.hpp"
#include "application/reference_images.hpp"
#include "application/radial_menu_state.hpp"
#include "application/console_view_state.hpp"
#include "application/transform_tool.hpp"
#include "application/project_state.hpp"
#include "application/modal_state.hpp"
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
    ReferenceTextureCache referenceTextures;
    FrameTimer frameTimer;

    RadialMenuState radialMenu;
    ConsoleViewState consoleView;
    ProjectState project;
    ModalState modal;

    // Import… was picked in the Objects tab; the file dialog opens in checkActions, not while drawing
    bool importRequested = false;
    // Likewise for + in the Images tab
    bool referenceRequested = false;
    // Where the image dialog last picked from (this session only)
    std::filesystem::path referenceFolder;

    // Where Tab returns to from object mode
    u32 lastEditMode = InputContext_SelectionVertex;

    bool is_running = false;
};