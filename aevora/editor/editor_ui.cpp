#include "editor/editor.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>

#include "ui/ui_style.hpp"

namespace {
    constexpr f32 TOP_BAR_HEIGHT = 30.0f;
    constexpr f32 TAB_PADDING = 18.0f;
    constexpr f32 BAR_INSET = 6.0f;
    constexpr f32 BUTTON_WIDTH = 64.0f;
    constexpr f32 STATUS_PADDING_X = 12.0f;
    constexpr f32 STATUS_PADDING_Y = 5.0f;
    constexpr f32 STATUS_GAP = 14.0f;
    constexpr f32 START_WIDTH = 380.0f;
    constexpr f32 START_HEIGHT = 166.0f;
    constexpr f64 ERROR_SHOWN = 5.0;

    std::string pathText(const std::filesystem::path& path) {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    f32 statusBarHeight(const UIFont& font) {
        return font.glyphHeight + STATUS_PADDING_Y * 2.0f;
    }

    // Workspace tabs on the left (once a project is open), the project and its buttons on the right
    void topBar(EditorContext& ctx, const Rect& bar) {
        UIContext& ui = ctx.ui;
        ui.beginRegion(bar);
        ui.drawList().rect(bar, UIStyle::PANEL_HEADER);
        ui.drawList().rect({ bar.x, bar.bottom() - 1.0f, bar.width, 1.0f }, UIStyle::PANEL_BORDER);

        f32 x = bar.x + BAR_INSET;
        for (u32 i = 0; ctx.project && i < static_cast<u32>(Workspace::Count); ++i) {
            const Workspace workspace = static_cast<Workspace>(i);
            const char* name = workspaceName(workspace);
            const f32 width = measureText(ui.font(), name).x + TAB_PADDING * 2.0f;
            if (ui.tab(name, { x, bar.y, width, bar.height - 1.0f }, ctx.workspace == workspace)) ctx.workspace = workspace;
            x += width;
        }

        const f32 buttonY = bar.y + std::floor((bar.height - UIStyle::ROW_HEIGHT) * 0.5f);
        f32 right = bar.right() - BAR_INSET - BUTTON_WIDTH;
        if (ui.button("Open", { right, buttonY, BUTTON_WIDTH, UIStyle::ROW_HEIGHT })) ctx.request = Request::OpenProject;
        right -= BUTTON_WIDTH + BAR_INSET;
        if (ui.button("New", { right, buttonY, BUTTON_WIDTH, UIStyle::ROW_HEIGHT })) ctx.request = Request::NewProject;

        if (ctx.project) {
            const f32 room = right - BAR_INSET * 2.0f - x;
            const std::string name = fitText(ui.font(), ctx.project->name, room);
            const f32 width = measureText(ui.font(), name).x;
            ui.text({ right - BAR_INSET * 2.0f - width, bar.y, width, bar.height }, name, UIStyle::TEXT_DIM);
        }
        ui.endRegion();
    }

    // Shown until a project is open: the two ways to get one
    void startScreen(EditorContext& ctx, const Rect& content) {
        UIContext& ui = ctx.ui;
        const Rect card {
            std::floor(content.x + (content.width - START_WIDTH) * 0.5f), std::floor(content.y + (content.height - START_HEIGHT) * 0.5f),
            START_WIDTH, START_HEIGHT
        };
        ui.beginRegion(card);
        ui.drawList().shadow({ card.x, card.y + UIStyle::PANEL_SHADOW_OFFSET, card.width, card.height }, UIStyle::PANEL_RADIUS, UIStyle::PANEL_SHADOW_BLUR, UIStyle::PANEL_SHADOW);
        ui.drawList().roundedRect(card, UIStyle::PANEL_RADIUS, UIStyle::PANEL_BACKGROUND, UIStyle::PANEL_BORDER, 1.0f);

        const f32 inner = card.width - UIStyle::PADDING * 4.0f;
        const f32 left = card.x + UIStyle::PADDING * 2.0f;
        f32 y = card.y + UIStyle::PADDING * 2.0f;
        ui.text({ left, y, inner, UIStyle::ROW_HEIGHT }, "Aevora Engine", UIStyle::ACCENT_GREEN);
        y += UIStyle::ROW_HEIGHT;
        ui.text({ left, y, inner, UIStyle::ROW_HEIGHT }, "No project is open", UIStyle::TEXT_DIM);
        y += UIStyle::ROW_HEIGHT + UIStyle::SECTION_SPACING;

        if (ui.button("New project    Ctrl+N", { left, y, inner, UIStyle::ROW_HEIGHT + 6.0f })) ctx.request = Request::NewProject;
        y += UIStyle::ROW_HEIGHT + 6.0f + UIStyle::ITEM_SPACING * 2.0f;
        if (ui.button("Open project   Ctrl+O", { left, y, inner, UIStyle::ROW_HEIGHT + 6.0f })) ctx.request = Request::OpenProject;
        ui.endRegion();
    }

    // Nothing lives in the workspaces yet; each says which one it is
    void workspaceArea(EditorContext& ctx, const Rect& content) {
        const UIFont& font = ctx.ui.font();
        const std::string text = std::string(workspaceName(ctx.workspace)) + ": nothing here yet";
        const Vec2 size = measureText(font, text);
        ctx.drawList.text(Vec2(std::floor(content.x + (content.width - size.x) * 0.5f), std::floor(content.y + (content.height - size.y) * 0.5f)), text, font, UIStyle::TEXT_DIM);
    }

    void statusBar(EditorContext& ctx, const UIFont& font, const Rect& bar) {
        UIDrawList& list = ctx.drawList;
        list.rect(bar, UIStyle::PANEL_HEADER);
        list.rect({ bar.x, bar.y, bar.width, 1.0f }, UIStyle::PANEL_BORDER);

        const f32 textY = bar.y + STATUS_PADDING_Y;
        f32 x = bar.x + STATUS_PADDING_X;
        const auto item = [&](const std::string& text, const Color& color) {
            if (x > bar.x + STATUS_PADDING_X) list.rect({ x - STATUS_GAP * 0.5f, textY, 1.0f, font.glyphHeight }, UIStyle::SEPARATOR);
            list.text(Vec2(x, textY), text, font, color);
            x += measureText(font, text).x + STATUS_GAP;
        };

        char fps[32];
        std::snprintf(fps, sizeof(fps), "%3.0f FPS", ctx.frameTimer.getFps());
        item(fps, UIStyle::TEXT_DIM);
        if (ctx.project) {
            item(workspaceName(ctx.workspace), UIStyle::TEXT);
            item(pathText(ctx.project->folder()), UIStyle::TEXT_DIM);
        }

        // Errors that arrive while the console is closed flash here; the open console shows them itself
        const bool consoleOpen = ctx.contexts.isActive(InputContext_Console);
        const f64 time = std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
        static u32 seenErrors = 0;
        static f64 shownAt = -ERROR_SHOWN;
        if (ctx.console.getErrorCount() != seenErrors) {
            seenErrors = ctx.console.getErrorCount();
            if (!consoleOpen) shownAt = time;
        }
        if (consoleOpen || time - shownAt > ERROR_SHOWN) return;

        const std::string& error = ctx.console.getLatestError();
        list.text(Vec2(x, textY), fitText(font, "! " + error.substr(0, error.find('\n')), bar.right() - STATUS_PADDING_X - x), font, UIStyle::ERROR);
    }
}

void Editor::drawInterface(EditorContext& ctx) {
    const UIFont font = makeUIFont(FontId::UI, ctx.fonts.get(FontId::UI));
    const Rect screen { 0.0f, 0.0f, static_cast<f32>(ctx.width), static_cast<f32>(ctx.height) };
    const f32 statusHeight = statusBarHeight(font);
    const Rect top { 0.0f, 0.0f, screen.width, TOP_BAR_HEIGHT };
    const Rect status { 0.0f, screen.height - statusHeight, screen.width, statusHeight };
    const Rect content { 0.0f, top.bottom(), screen.width, std::max(0.0f, status.y - top.bottom()) };

    ctx.ui.setDrawList(&ctx.drawList);
    ctx.ui.setFont(font);
    ctx.ui.setViewport(screen);
    ctx.ui.beginDraw();
    topBar(ctx, top);
    if (ctx.project) workspaceArea(ctx, content);
    else startScreen(ctx, content);
    ctx.ui.endDraw();

    statusBar(ctx, font, status);

    if (ctx.contexts.isActive(InputContext_Console)) drawConsole(ctx.console, ctx.consoleView, ctx.fonts, ctx.input, ctx.drawList, content);
}
