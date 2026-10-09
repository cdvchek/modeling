#include "application/ui/top_bar.hpp"
#include "application/uv/uv_editor.hpp"
#include "application/uv/uv_operations.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>

namespace {
    // Paint comes with texture painting
    constexpr Workspace TABS[] = { Workspace::Model, Workspace::UV };
    constexpr f32 TAB_PADDING = 18.0f;
    constexpr f32 BAR_INSET = 6.0f;
    constexpr f32 OBJECT_PICKER_WIDTH = 220.0f;
    constexpr f32 TEXTURE_PICKER_WIDTH = 200.0f;
    constexpr f32 HEADER_GAP = 24.0f;

    void topBar(AppContext& ctx, const Rect& bar) {
        UIContext& ui = ctx.ui;
        ui.beginRegion(bar);
        ui.drawList().rect(bar, UIStyle::PANEL_HEADER);
        ui.drawList().rect({ bar.x, bar.bottom() - 1.0f, bar.width, 1.0f }, UIStyle::PANEL_BORDER);

        f32 x = bar.x + BAR_INSET;
        for (Workspace workspace : TABS) {
            const char* name = workspaceName(workspace);
            const f32 width = measureText(ui.font(), name).x + TAB_PADDING * 2.0f;
            const Rect tab { x, bar.y, width, bar.height - 1.0f };
            if (ui.tab(name, tab, ctx.workspace.current == workspace) && !setWorkspace(ctx, workspace)) {
                ctx.systems.console.printError("Finish or cancel the tool before switching workspaces");
            }
            x += width;
        }
        ui.endRegion();
    }

    // Above both views: which object is being UV-edited
    void uvHeader(AppContext& ctx, const Rect& header) {
        UIContext& ui = ctx.ui;
        ui.beginRegion(header);
        ui.drawList().rect(header, UIStyle::PANEL_BACKGROUND);
        ui.drawList().rect({ header.x, header.bottom() - 1.0f, header.width, 1.0f }, UIStyle::PANEL_BORDER);

        const Rect row { header.x + BAR_INSET * 2.0f, header.y + std::floor((header.height - UIStyle::ROW_HEIGHT) * 0.5f), 0.0f, UIStyle::ROW_HEIGHT };
        const ObjectCollection& objects = ctx.scene.objects;
        if (objects.count() == 0) {
            ui.text({ row.x, row.y, header.width, row.height }, "No objects: add one in the Model workspace", UIStyle::TEXT_DIM);
            ui.endRegion();
            return;
        }

        const std::string_view label = "Object";
        const f32 labelWidth = measureText(ui.font(), label).x + BAR_INSET * 2.0f;
        ui.text({ row.x, row.y, labelWidth, row.height }, label, UIStyle::TEXT_DIM);

        // Every object by name; picking one makes it the object being edited, with nothing of it selected yet
        const std::vector<ObjectHandle> handles = objects.handles();
        std::vector<std::string_view> names;
        i32 current = 0;
        for (u32 i = 0; i < handles.size(); ++i) {
            names.push_back(objects.get(handles[i]).name);
            if (handles[i] == uvObject(ctx)) current = static_cast<i32>(i);
        }
        if (ui.dropdown("uv object", { row.x + labelWidth, row.y, OBJECT_PICKER_WIDTH, row.height }, current, names)) {
            Selection& selection = ctx.scene.selection;
            selection.clearMeshElements();
            selection.setActiveObject(handles[current]);
            // A different object's UVs get framed afresh
            ctx.workspace.uvZoom = 0.0f;
        }
        f32 x = row.x + labelWidth + OBJECT_PICKER_WIDTH + HEADER_GAP;

        // What shows behind the UVs: the material's map, a checker, or any texture
        WorkspaceState& workspace = ctx.workspace;
        const TextureCollection& textures = ctx.scene.textures;
        const std::vector<TextureHandle> textureHandles = textures.handles();
        std::vector<std::string_view> backgrounds = { "From material", "Checker" };
        std::vector<u32> icons = { 0, 0 };
        i32 background = workspace.uvBackground == UVBackground::Checker ? 1 : 0;
        for (u32 i = 0; i < textureHandles.size(); ++i) {
            backgrounds.push_back(textures.get(textureHandles[i]).name);
            icons.push_back(ctx.pictureTextures.find(textures.get(textureHandles[i]).picture));
            if (workspace.uvBackground == UVBackground::Texture && workspace.uvTexture == textureHandles[i]) background = static_cast<i32>(i) + 2;
        }
        const std::string_view textureLabel = "Texture";
        const f32 textureLabelWidth = measureText(ui.font(), textureLabel).x + BAR_INSET * 2.0f;
        ui.text({ x, row.y, textureLabelWidth, row.height }, textureLabel, UIStyle::TEXT_DIM);
        if (ui.dropdown("uv background", { x + textureLabelWidth, row.y, TEXTURE_PICKER_WIDTH, row.height }, background, backgrounds, icons)) {
            workspace.uvBackground = background == 0 ? UVBackground::Material : background == 1 ? UVBackground::Checker : UVBackground::Texture;
            workspace.uvTexture = background >= 2 ? textureHandles[background - 2] : INVALID_TEXTURE;
        }
        x += textureLabelWidth + TEXTURE_PICKER_WIDTH + HEADER_GAP;

        // The grid over the editor
        const std::string_view gridLabel = "Grid";
        const f32 gridLabelWidth = measureText(ui.font(), gridLabel).x + BAR_INSET * 2.0f;
        ui.text({ x, row.y, gridLabelWidth, row.height }, gridLabel, UIStyle::TEXT_DIM);
        ui.checkbox("uv grid", { x + gridLabelWidth, row.y, row.height, row.height }, workspace.uvGrid);
        ui.endRegion();
    }

    void drawUVToolsPanel(AppContext& ctx, const Rect& area) {
        UIContext& ui = ctx.ui;
        ui.drawList().rect(area, UIStyle::PANEL_BACKGROUND);
        ui.drawList().rect({ area.x, area.y, 1.0f, area.height }, UIStyle::PANEL_BORDER);
        const bool object = hasUVObject(ctx);

        // Seams: edges unwrapping cuts along, marked in edge mode
        ui.heading("Seams");
        const bool edges = canMarkSeams(ctx);
        if (ui.button("Mark seam", ui.row(), edges)) markSeams(ctx, true);
        if (ui.button("Clear seam", ui.row(), edges)) markSeams(ctx, false);
        if (!edges) ui.label("Edges, in edge mode", true);
        if (ui.button("From islands", ui.row(), object)) seamsFromIslands(ctx);
        ui.spacing();

        // Unwrap the selected faces (or all), cutting along seams
        ui.heading("Unwrap");
        const bool everything = object && uvTargetFaces(ctx).size() == ctx.scene.objects.get(uvObject(ctx)).meshData.getFaceHandles().size();
        if (ui.button(everything ? "Unwrap all (U)" : "Unwrap selected (U)", ui.row(), object)) unwrapUVs(ctx);
        ui.spacing();

        ui.heading("Project");
        if (ui.button("From view", ui.row(), object)) projectUVs(ctx, UVProjection::View);
        if (ui.button("Box", ui.row(), object)) projectUVs(ctx, UVProjection::Box);
        if (ui.button("Cylinder", ui.row(), object)) projectUVs(ctx, UVProjection::Cylinder);
        if (ui.button("Sphere", ui.row(), object)) projectUVs(ctx, UVProjection::Sphere);
        ui.spacing();

        // Pack every island; the margin is in percent of the texture
        ui.heading("Pack");
        f32& margin = ctx.workspace.uvMargin;
        if (ui.dragFloat("Gap", margin, 0.05f, "%.1f %%")) margin = std::clamp(margin, 0.0f, 10.0f);
        if (ui.button("Pack islands", ui.row(), object)) packUVs(ctx);
    }

    void uvSide(AppContext& ctx, const ScreenLayout& layout) {
        UIContext& ui = ctx.ui;

        // Dragging the divider moves the split; the layout keeps both sides wide enough
        ui.beginRegion(layout.divider);
        f32 x = layout.divider.x;
        if (ui.splitter("uv divider", layout.divider, x)) {
            const f32 usable = std::max(1.0f, layout.content.width - layout.uvTools.width - DIVIDER_WIDTH);
            ctx.workspace.uvSplit = std::clamp((x - layout.content.x) / usable, 0.0f, 1.0f);
        }
        ui.endRegion();

        // The UV editor: a region, so clicks there stay out of the 3D view
        ui.beginRegion(layout.uvEditor);
        drawUVEditor(ctx, layout.uvEditor);
        ui.endRegion();

        // The tools column: seams, unwrapping, projections, packing
        ui.beginRegion(layout.uvTools);
        drawUVToolsPanel(ctx, layout.uvTools);
        ui.endRegion();
    }
}

void drawWorkspaceChrome(AppContext& ctx, const ScreenLayout& layout) {
    topBar(ctx, layout.topBar);
    if (ctx.workspace.current == Workspace::UV) {
        uvHeader(ctx, layout.header);
        uvSide(ctx, layout);
    }
}
