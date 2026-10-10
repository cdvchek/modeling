#include "application/workspace.hpp"
#include "application/app_context.hpp"
#include "application/ui/status_bar.hpp"
#include "core/math/projection.hpp"
#include "application/actions/editing_actions.hpp"

#include <algorithm>

namespace {
    constexpr u32 TOOL_CONTEXTS = InputContext_Grab | InputContext_Scale | InputContext_Rotate | InputContext_Bevel | InputContext_Inset;
}

const char* workspaceName(Workspace workspace) {
    switch (workspace) {
        case Workspace::Model: return "Model";
        case Workspace::UV: return "UV";
        case Workspace::Paint: return "Paint";
    }
    return "";
}

ScreenLayout screenLayout(const AppContext& ctx) {
    ScreenLayout layout;
    u32 width = 0;
    u32 height = 0;
    if (!ctx.windows.empty()) ctx.windows[0]->getDimensions(width, height);
    layout.window = { 0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height) };

    // Top bar, then everything else down to the status bar
    const f32 top = std::min(TOP_BAR_HEIGHT, layout.window.height);
    const f32 bottom = std::max(top, layout.window.height - statusBarHeight(ctx));
    layout.topBar = { 0.0f, 0.0f, layout.window.width, top };
    layout.content = { 0.0f, top, layout.window.width, bottom - top };
    layout.scene = layout.content;

    if (ctx.workspace.current == Workspace::UV) {
        // The header takes the top of the content; the views share what's below it
        layout.header = { layout.content.x, layout.content.y, layout.content.width, std::min(WORKSPACE_HEADER_HEIGHT, layout.content.height) };
        // The tools column takes the right edge (narrower when the window is)
        const f32 toolsWidth = std::min(TOOLS_WIDTH, std::max(0.0f, layout.content.width * 0.3f));
        const f32 below = layout.content.height - layout.header.height;
        layout.uvTools = { layout.content.right() - toolsWidth, layout.header.bottom(), toolsWidth, below };
        const Rect area { layout.content.x, layout.header.bottom(), layout.content.width - toolsWidth, below };

        // The divider stays where the split says, kept so both sides have room when the window allows it
        const f32 usable = std::max(0.0f, area.width - DIVIDER_WIDTH);
        const f32 minSide = std::min(MIN_SPLIT_WIDTH, usable * 0.5f);
        const f32 left = std::clamp(std::floor(usable * ctx.workspace.uvSplit), minSide, usable - minSide);

        layout.scene = { area.x, area.y, left, area.height };
        layout.divider = { area.x + left, area.y, DIVIDER_WIDTH, area.height };
        layout.uvEditor = { layout.divider.right(), area.y, area.right() - layout.divider.right(), area.height };
    } else if (ctx.workspace.current == Workspace::Paint) {
        // A header, the tools column at the right edge, and one viewport for the rest
        layout.header = { layout.content.x, layout.content.y, layout.content.width, std::min(WORKSPACE_HEADER_HEIGHT, layout.content.height) };
        const f32 toolsWidth = std::min(PAINT_TOOLS_WIDTH, std::max(0.0f, layout.content.width * 0.3f));
        const f32 below = layout.content.height - layout.header.height;
        layout.paintTools = { layout.content.right() - toolsWidth, layout.header.bottom(), toolsWidth, below };
        layout.scene = { layout.content.x, layout.header.bottom(), layout.content.width - toolsWidth, below };
        if (ctx.workspace.paint2D) layout.paintCanvas = layout.scene;
    }
    return layout;
}

Rect sceneView(const AppContext& ctx) {
    return screenLayout(ctx).scene;
}

Mat4 sceneViewProjection(const AppContext& ctx) {
    const Rect view = sceneView(ctx);
    const f32 aspect = view.width > 0.0f && view.height > 0.0f ? view.width / view.height : 1.0f;
    return ctx.scene.camera.getProjectionMatrix(aspect) * ctx.scene.camera.getViewMatrix();
}

bool projectToView(const Mat4& viewProjection, const Vec3& point, const Rect& view, Vec2& out) {
    if (!projectToScreen(viewProjection, point, view.width, view.height, out)) return false;
    out = out + Vec2(view.x, view.y);
    return true;
}

bool setWorkspace(AppContext& ctx, Workspace workspace) {
    if ((ctx.systems.input_ctx.getContext() & TOOL_CONTEXTS) || ctx.uvTool.active()) return false;
    ctx.workspace.current = workspace;
    ctx.radialMenu.open = false;

    if (workspace != Workspace::Model) {
        Selection& selection = ctx.scene.selection;
        if (!ctx.scene.objects.isValid(selection.getActiveObject()) && ctx.scene.objects.count() > 0) {
            selection.setActiveObject(ctx.scene.objects.handles().front());
        }
        selection.clearLights();
        selection.clearReferences();
        selection.clearOrigin();
        if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionObject) setSelectionMode(ctx, ctx.lastEditMode);
    }
    // Island mode is UV's own
    if (workspace != Workspace::UV) ctx.workspace.uvIslands = false;
    return true;
}

bool actionAllowed(Workspace workspace, Action action) {
    // Framing belongs to UV for now (F is Fill in Model)
    if (workspace == Workspace::Model) {
        return action != Action::FrameSelected && action != Action::FrameAll && action != Action::IslandMode && action != Action::SelectAll
            && action != Action::UVGrab && action != Action::UVScale && action != Action::UVRotate && action != Action::UVUnwrap
            && action != Action::TogglePaintView && action != Action::PickPaintTexture
            && action != Action::PaintBrush && action != Action::PaintEraser && action != Action::BrushSmaller && action != Action::BrushLarger;
    }

    // What every workspace but Model shares: quitting, the console, the camera, undo, files, modal windows, framing
    switch (action) {
        case Action::Quit:
        case Action::ToggleConsole:
        case Action::EnterCommand:
        case Action::ConsoleBackspace:
        case Action::ConsoleDelete:
        case Action::ConsoleCursorLeft:
        case Action::ConsoleCursorRight:
        case Action::ConsoleHistoryOlder:
        case Action::ConsoleHistoryNewer:
        case Action::ViewportOrbit:
        case Action::ViewportPan:
        case Action::ViewportZoom:
        case Action::Undo:
        case Action::Redo:
        case Action::SaveProject:
        case Action::SaveProjectAs:
        case Action::OpenProject:
        case Action::NewProject:
        case Action::ModalConfirm:
        case Action::ModalCancel:
        case Action::FrameSelected:
        case Action::FrameAll:
            return true;
        default:
            break;
    }

    // Paint shows materials always, so only the UV checker toggles; nothing selects yet
    if (workspace == Workspace::Paint) {
        return action == Action::TogglePaintView || action == Action::PickPaintTexture || action == Action::ToggleUVChecker
            || action == Action::PaintBrush || action == Action::PaintEraser || action == Action::BrushSmaller || action == Action::BrushLarger;
    }

    switch (action) {
        case Action::VertexMode:
        case Action::EdgeMode:
        case Action::FaceMode:
        case Action::Select:
        case Action::ToggleSelection:
        case Action::SelectLoop:
        case Action::SelectRing:
        case Action::ToggleUVChecker:
        case Action::ToggleMaterials:
        case Action::IslandMode:
        case Action::SelectAll:
        case Action::UVGrab:
        case Action::UVScale:
        case Action::UVRotate:
        case Action::UVUnwrap:
            return true;
        default:
            return false;
    }
}

bool isModelCommand(const std::string& name) {
    for (const char* model : { "merge", "dissolve", "light", "material", "texture", "shading", "reference", "origin", "import", "object" }) {
        if (name == model) return true;
    }
    return false;
}

ObjectHandle uvObject(const AppContext& ctx) {
    const ObjectHandle active = ctx.scene.selection.getActiveObject();
    return ctx.scene.objects.isValid(active) ? active : INVALID_OBJECT;
}
