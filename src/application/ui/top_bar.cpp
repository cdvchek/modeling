#include "application/ui/top_bar.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>

namespace {
    // Paint comes with texture painting
    constexpr Workspace TABS[] = { Workspace::Model, Workspace::UV };
    constexpr f32 TAB_PADDING = 18.0f;
    constexpr f32 BAR_INSET = 6.0f;

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

    void uvSide(AppContext& ctx, const ScreenLayout& layout) {
        UIContext& ui = ctx.ui;

        // Dragging the divider moves the split; the layout keeps both sides wide enough
        ui.beginRegion(layout.divider);
        f32 x = layout.divider.x;
        if (ui.splitter("uv divider", layout.divider, x)) {
            const f32 usable = std::max(1.0f, layout.content.width - DIVIDER_WIDTH);
            ctx.workspace.uvSplit = std::clamp((x - layout.content.x) / usable, 0.0f, 1.0f);
        }
        ui.endRegion();

        // The UV editor comes next; for now its area is empty
        ui.beginRegion(layout.uvEditor);
        ui.drawList().rect(layout.uvEditor, UIStyle::PANEL_BACKGROUND);
        const std::string_view label = "UV editor";
        const f32 textWidth = measureText(ui.font(), label).x;
        ui.text({ layout.uvEditor.x + std::floor((layout.uvEditor.width - textWidth) * 0.5f), layout.uvEditor.center().y - 10.0f, textWidth, 20.0f },
                label, UIStyle::TEXT_DIM);
        ui.endRegion();
    }
}

void drawWorkspaceChrome(AppContext& ctx, const ScreenLayout& layout) {
    topBar(ctx, layout.topBar);
    if (ctx.workspace.current == Workspace::UV) uvSide(ctx, layout);
}
