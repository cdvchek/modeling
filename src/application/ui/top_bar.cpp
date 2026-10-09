#include "application/ui/top_bar.hpp"
#include "application/uv/uv_editor.hpp"
#include "application/uv/uv_operations.hpp"
#include "application/paint/paint_workspace.hpp"
#include "scene/textures/paint_targets.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>

namespace {
    constexpr Workspace TABS[] = { Workspace::Model, Workspace::UV, Workspace::Paint };
    constexpr f32 TAB_PADDING = 18.0f;
    constexpr f32 BAR_INSET = 6.0f;
    constexpr f32 OBJECT_PICKER_WIDTH = 220.0f;
    constexpr f32 TEXTURE_PICKER_WIDTH = 200.0f;
    constexpr f32 PAINT_TEXTURE_PICKER_WIDTH = 280.0f;
    constexpr f32 VIEW_SWITCH_WIDTH = 96.0f;
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

    // A dim label at x in the row; returns where what it labels starts
    f32 headerLabel(UIContext& ui, std::string_view label, f32 x, const Rect& row) {
        const f32 width = measureText(ui.font(), label).x + BAR_INSET * 2.0f;
        ui.text({ x, row.y, width, row.height }, label, UIStyle::TEXT_DIM);
        return x + width;
    }

    // A header's background and its row of controls; false (with a note in place of the controls) when there are no
    // objects to work on, and then the region is already closed
    bool beginHeader(AppContext& ctx, const Rect& header, Rect& row) {
        UIContext& ui = ctx.ui;
        ui.beginRegion(header);
        ui.drawList().rect(header, UIStyle::PANEL_BACKGROUND);
        ui.drawList().rect({ header.x, header.bottom() - 1.0f, header.width, 1.0f }, UIStyle::PANEL_BORDER);

        row = { header.x + BAR_INSET * 2.0f, header.y + std::floor((header.height - UIStyle::ROW_HEIGHT) * 0.5f), 0.0f, UIStyle::ROW_HEIGHT };
        if (ctx.scene.objects.count() > 0) return true;
        ui.text({ row.x, row.y, header.width, row.height }, "No objects: add one in the Model workspace", UIStyle::TEXT_DIM);
        ui.endRegion();
        return false;
    }

    // Every object by name; picking one makes it the object being worked on, with nothing of it selected yet, and
    // both flat views frame it afresh. Returns where the next control goes.
    f32 objectPicker(AppContext& ctx, const Rect& row) {
        UIContext& ui = ctx.ui;
        const f32 x = headerLabel(ui, "Object", row.x, row);
        const ObjectCollection& objects = ctx.scene.objects;
        const std::vector<ObjectHandle> handles = objects.handles();
        std::vector<std::string_view> names;
        i32 current = 0;
        for (u32 i = 0; i < handles.size(); ++i) {
            names.push_back(objects.get(handles[i]).name);
            if (handles[i] == uvObject(ctx)) current = static_cast<i32>(i);
        }
        if (ui.dropdown("object", { x, row.y, OBJECT_PICKER_WIDTH, row.height }, current, names)) {
            Selection& selection = ctx.scene.selection;
            selection.clearMeshElements();
            selection.setActiveObject(handles[current]);
            ctx.workspace.uvView.zoom = 0.0f;
            ctx.workspace.paintView.zoom = 0.0f;
        }
        return x + OBJECT_PICKER_WIDTH + HEADER_GAP;
    }

    // Above both views: which object is being UV-edited
    void uvHeader(AppContext& ctx, const Rect& header) {
        UIContext& ui = ctx.ui;
        Rect row;
        if (!beginHeader(ctx, header, row)) return;
        f32 x = objectPicker(ctx, row);

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
        x = headerLabel(ui, "Texture", x, row);
        if (ui.dropdown("uv background", { x, row.y, TEXTURE_PICKER_WIDTH, row.height }, background, backgrounds, icons)) {
            workspace.uvBackground = background == 0 ? UVBackground::Material : background == 1 ? UVBackground::Checker : UVBackground::Texture;
            workspace.uvTexture = background >= 2 ? textureHandles[background - 2] : INVALID_TEXTURE;
        }
        x += TEXTURE_PICKER_WIDTH + HEADER_GAP;

        // The grid over the editor
        x = headerLabel(ui, "Grid", x, row);
        ui.checkbox("uv grid", { x, row.y, row.height, row.height }, workspace.uvGrid);
        ui.endRegion();
    }

    // Above the viewport: the object, the texture being painted, and 3D or 2D
    void paintHeader(AppContext& ctx, const Rect& header) {
        UIContext& ui = ctx.ui;
        Rect row;
        if (!beginHeader(ctx, header, row)) return;
        f32 x = objectPicker(ctx, row);

        // Each texture the object's materials use, with those materials; then New texture for each material without one
        const std::vector<PaintTarget> targets = paintTargets(ctx.scene, uvObject(ctx));
        const TextureHandle active = activePaintTexture(ctx);
        std::vector<std::string> labels;
        std::vector<u32> icons;
        i32 current = -1;
        for (const PaintTarget& target : targets) {
            const std::string& material = ctx.scene.materials.get(target.materials.front()).name;
            if (!ctx.scene.textures.isValid(target.texture)) {
                labels.push_back("New texture for " + material);
                icons.push_back(0);
                continue;
            }
            const Texture& texture = ctx.scene.textures.get(target.texture);
            std::string label = texture.name + " (" + material;
            for (std::size_t i = 1; i < target.materials.size(); ++i) label += ", " + ctx.scene.materials.get(target.materials[i]).name;
            labels.push_back(label + ")");
            icons.push_back(ctx.pictureTextures.find(texture.picture));
            if (target.texture == active) current = static_cast<i32>(labels.size()) - 1;
        }
        const std::vector<std::string_view> names(labels.begin(), labels.end());

        x = headerLabel(ui, "Texture", x, row);
        i32 picked = current;
        if (ui.dropdown("paint texture", { x, row.y, PAINT_TEXTURE_PICKER_WIDTH, row.height }, picked, names, icons) && picked >= 0) {
            const PaintTarget& target = targets[picked];
            if (ctx.scene.textures.isValid(target.texture)) ctx.workspace.paintTexture = target.texture;
            else openNewTextureWindow(ctx, target.materials.front());
        }
        x += PAINT_TEXTURE_PICKER_WIDTH + HEADER_GAP;

        // The model or the flat texture (Tab)
        x = headerLabel(ui, "View", x, row);
        i32 view = ctx.workspace.paint2D ? 1 : 0;
        if (ui.segmented("paint view", { x, row.y, VIEW_SWITCH_WIDTH, row.height }, view, { "3D", "2D" })) ctx.workspace.paint2D = view == 1;
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
    } else if (ctx.workspace.current == Workspace::Paint) {
        paintHeader(ctx, layout.header);

        // The flat texture is a region, so the camera never moves under it
        if (layout.paintCanvas.width > 0.0f) {
            ctx.ui.beginRegion(layout.paintCanvas);
            drawPaintCanvas(ctx, layout.paintCanvas);
            ctx.ui.endRegion();
        }
        ctx.ui.beginRegion(layout.paintTools);
        drawPaintToolsPanel(ctx, layout.paintTools);
        ctx.ui.endRegion();
    }
}
