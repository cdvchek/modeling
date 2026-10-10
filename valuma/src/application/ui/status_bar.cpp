#include "application/ui/status_bar.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace {
    constexpr f32 PADDING_X = 12.0f;
    constexpr f32 PADDING_Y = 6.0f;
    constexpr f32 ITEM_GAP = 12.0f;

    const Color BAR_COLOR { 0.10f, 0.10f, 0.13f, 0.95f };
    const Color BORDER_COLOR = UIStyle::PANEL_BORDER;
    const Color DIVIDER_COLOR = UIStyle::SEPARATOR;
    const Color TEXT_COLOR = UIStyle::TEXT;
    const Color DIM_TEXT_COLOR = UIStyle::TEXT_DIM;
    const Color ERROR_COLOR = UIStyle::ERROR;

    // A new error shows at the right end for a few seconds, fading out at the end
    constexpr f64 ERROR_SHOWN = 4.0;
    constexpr f64 ERROR_FADE = 0.6;

    // Same hues as the grid axes
    const Color X_AXIS_COLOR = UIStyle::AXIS_X;
    const Color Y_AXIS_COLOR = UIStyle::AXIS_Y;
    const Color Z_AXIS_COLOR = UIStyle::AXIS_Z;

    const std::vector<std::string_view> MODE_NAMES = { "Vertex", "Edge", "Face", "Object", "Island" };
    const std::vector<std::string_view> TOOL_NAMES = { "Select", "Grab", "Scale", "Rotate", "Bevel", "Inset" };
    const std::vector<std::string_view> AXIS_VALUES = { "Free", "XYZ", "-" };
    constexpr std::string_view AXIS_LABEL = "Axis ";
    constexpr std::string_view FPS_LABEL = " FPS";
    constexpr std::size_t FPS_DIGITS = 4;

    std::size_t longest(const std::vector<std::string_view>& names) {
        std::size_t length = 0;
        for (std::string_view name : names) length = std::max(length, name.size());
        return length;
    }

    struct TextSegment {
        std::string text;
        Color color;
    };

    // An item reserves room for its longest possible value so later items never shift
    struct StatusItem {
        std::vector<TextSegment> segments;
        std::size_t reservedChars;
    };

    StatusItem fpsItem(f32 fps) {
        return { { { std::to_string(std::lround(fps)), TEXT_COLOR }, { std::string(FPS_LABEL), DIM_TEXT_COLOR } }, FPS_DIGITS + FPS_LABEL.size() };
    }

    StatusItem axisLockItem(const ContextManager& contexts) {
        StatusItem item { { { std::string(AXIS_LABEL), DIM_TEXT_COLOR } }, AXIS_LABEL.size() + longest(AXIS_VALUES) };

        const bool lockable = contexts.isActive(InputContext_Grab) || contexts.isActive(InputContext_Scale) || contexts.isActive(InputContext_Rotate);
        if (!lockable) {
            item.segments.push_back({ "-", DIM_TEXT_COLOR });
            return item;
        }

        if (contexts.isActive(InputContext_XAxis)) item.segments.push_back({ "X", X_AXIS_COLOR });
        if (contexts.isActive(InputContext_YAxis)) item.segments.push_back({ "Y", Y_AXIS_COLOR });
        if (contexts.isActive(InputContext_ZAxis)) item.segments.push_back({ "Z", Z_AXIS_COLOR });

        if (item.segments.size() == 1) item.segments.push_back({ "Free", TEXT_COLOR });
        return item;
    }
}

namespace {
    // A UV tool locks to u (X) or v (Y); rotating has nothing to lock
    StatusItem uvAxisItem(const UVToolState& tool) {
        StatusItem item { { { std::string(AXIS_LABEL), DIM_TEXT_COLOR } }, AXIS_LABEL.size() + longest(AXIS_VALUES) };
        if (tool.kind == UVToolKind::Rotate) item.segments.push_back({ "-", DIM_TEXT_COLOR });
        else if (tool.axis == UVAxis::X) item.segments.push_back({ "X", X_AXIS_COLOR });
        else if (tool.axis == UVAxis::Y) item.segments.push_back({ "Y", Y_AXIS_COLOR });
        else item.segments.push_back({ "Free", TEXT_COLOR });
        return item;
    }
}

const char* selectionModeName(const ContextManager& contexts) {
    switch (contexts.getModeContext()) {
        case InputContext_SelectionEdge: return MODE_NAMES[1].data();
        case InputContext_SelectionFace: return MODE_NAMES[2].data();
        case InputContext_SelectionObject: return MODE_NAMES[3].data();
    }
    return MODE_NAMES[0].data();
}

const char* activeToolName(const ContextManager& contexts) {
    if (contexts.isActive(InputContext_Grab)) return TOOL_NAMES[1].data();
    if (contexts.isActive(InputContext_Scale)) return TOOL_NAMES[2].data();
    if (contexts.isActive(InputContext_Rotate)) return TOOL_NAMES[3].data();
    if (contexts.isActive(InputContext_Bevel)) return TOOL_NAMES[4].data();
    if (contexts.isActive(InputContext_Inset)) return TOOL_NAMES[5].data();
    return TOOL_NAMES[0].data();
}

f32 statusBarHeight(const AppContext& ctx) {
    return static_cast<f32>(ctx.fonts.get(FontId::UI).getGlyphHeight()) + PADDING_Y * 2.0f;
}

void drawStatusBar(const AppContext& ctx, UIDrawList& ui, f32 width, f32 height) {
    const UIFont font = makeUIFont(FontId::UI, ctx.fonts.get(FontId::UI));
    const f32 barHeight = statusBarHeight(ctx);
    const f32 top = height - barHeight;

    ui.rect({ 0.0f, top, width, barHeight }, BAR_COLOR);
    ui.rect({ 0.0f, top, width, 1.0f }, BORDER_COLOR);

    const ContextManager& contexts = ctx.systems.input_ctx;
    std::vector<StatusItem> items = { fpsItem(ctx.frameTimer.getFps()) };
    if (ctx.workspace.current == Workspace::Paint) {
        // Which view shows, and the tool
        items.push_back({ { { ctx.workspace.paint2D ? "2D" : "3D", TEXT_COLOR } }, 2 });
        items.push_back({ { { ctx.workspace.brush.erase ? "Eraser" : "Brush", TEXT_COLOR } }, 6 });
    } else {
        items.push_back({ { { ctx.workspace.uvIslands ? MODE_NAMES[4].data() : selectionModeName(contexts), TEXT_COLOR } }, longest(MODE_NAMES) });
        items.push_back({ { { ctx.uvTool.active() ? uvToolName(ctx.uvTool.kind) : activeToolName(contexts), TEXT_COLOR } }, longest(TOOL_NAMES) });
        items.push_back(ctx.uvTool.active() ? uvAxisItem(ctx.uvTool) : axisLockItem(contexts));
    }

    // Left to right, with a divider between items
    f32 x = PADDING_X;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            ui.rect({ x, top + PADDING_Y, 1.0f, font.glyphHeight }, DIVIDER_COLOR);
            x += 1.0f + ITEM_GAP;
        }

        std::size_t chars = 0;
        for (const TextSegment& segment : items[i].segments) {
            ui.text(Vec2(x + chars * font.glyphWidth, top + PADDING_Y), segment.text, font, segment.color);
            chars += segment.text.size();
        }

        x += std::max(chars, items[i].reservedChars) * font.glyphWidth + ITEM_GAP;
    }

    // Errors that arrive while the console is closed flash here; the open console shows them itself
    const Console& console = ctx.systems.console;
    const bool consoleOpen = contexts.isActive(InputContext_Console);
    const f64 time = std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
    static u32 seenErrors = 0;
    static f64 shownAt = -ERROR_SHOWN;

    if (console.getErrorCount() != seenErrors) {
        seenErrors = console.getErrorCount();
        if (!consoleOpen) shownAt = time;
    }

    const f64 remaining = ERROR_SHOWN - (time - shownAt);
    if (consoleOpen || remaining <= 0.0) return;

    const std::string& error = console.getLatestError();
    std::string text = "! " + error.substr(0, error.find('\n'));

    // Cut it short rather than run into the other items
    const std::size_t room = static_cast<std::size_t>(std::max(0.0f, (width - PADDING_X - x) / font.glyphWidth));
    if (text.size() > room) text = room > 3 ? text.substr(0, room - 3) + "..." : std::string();

    Color color = ERROR_COLOR;
    color.a = static_cast<f32>(std::min(1.0, remaining / ERROR_FADE));
    ui.text(Vec2(width - PADDING_X - static_cast<f32>(text.size()) * font.glyphWidth, top + PADDING_Y), text, font, color);
}
