#include "application/ui/console_view.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string_view>

namespace {
    constexpr f32 MARGIN = 12.0f;
    constexpr f32 PADDING = 12.0f;
    constexpr f32 MAX_HEIGHT = 420.0f;
    constexpr f32 HEIGHT_FRACTION = 0.45f;
    constexpr f32 HEADER_HEIGHT = 30.0f;
    constexpr f32 LINE_GAP = 4.0f;
    constexpr f32 INPUT_PADDING_X = 10.0f;
    constexpr f32 INPUT_PADDING_Y = 6.0f;
    constexpr f32 CARET_WIDTH = 2.0f;
    constexpr f64 CARET_BLINK = 0.5;   // seconds on, then off
    constexpr f32 SCROLLBAR_SPACE = 8.0f;
    constexpr f32 CHEVRON = 4.0f;
    constexpr i32 WHEEL_DELTA_PER_LINE = 40;   // a wheel notch (120) scrolls three lines

    const Color PROMPT_DIM { 0.31f, 0.98f, 0.48f, 0.45f };
    const Color HISTORY_NEWEST { 0.90f, 0.90f, 0.92f, 1.0f };
    const Color HISTORY_OLDEST { 0.38f, 0.45f, 0.64f, 0.65f };
    const Color OUTPUT_TEXT { 0.74f, 0.77f, 0.88f, 1.0f };
    const Color ERROR_TEXT = UIStyle::ERROR;

    Color mix(const Color& a, const Color& b, f32 t) {
        return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
    }

    f64 now() {
        return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // One row of the list: a line of an entry; a collapsed entry is just its first line
    struct DisplayLine {
        u32 entry;
        std::string_view text;
        bool first;
        u32 hiddenLines;
    };

    // Splits a line into rows of at most width characters, breaking after a space when there is one
    std::vector<std::string_view> wrap(std::string_view text, std::size_t width) {
        std::vector<std::string_view> rows;
        while (text.size() > width && width > 0) {
            std::size_t cut = text.rfind(' ', width);
            cut = (cut == std::string_view::npos || cut == 0) ? width : cut + 1;
            rows.push_back(text.substr(0, cut));
            text.remove_prefix(cut);
        }
        rows.push_back(text);
        return rows;
    }

    // Lines longer than width characters wrap onto further rows; only an entry's very first row counts as first
    std::vector<DisplayLine> layoutLines(const std::vector<ConsoleEntry>& entries, std::size_t width) {
        std::vector<DisplayLine> lines;

        for (u32 i = 0; i < static_cast<u32>(entries.size()); ++i) {
            const std::string_view text = entries[i].text;
            const bool collapsed = entries[i].collapsible() && !entries[i].expanded;
            const u32 lineCount = static_cast<u32>(std::count(text.begin(), text.end(), '\n')) + 1;

            std::size_t start = 0;
            for (u32 line = 0; line < lineCount; ++line) {
                const std::size_t end = text.find('\n', start);
                const std::string_view part = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);

                const std::vector<std::string_view> rows = wrap(part, width);
                for (std::size_t r = 0; r < rows.size(); ++r) {
                    lines.push_back({ i, rows[r], line == 0 && r == 0, collapsed ? lineCount - 1 : 0 });
                    if (collapsed) break;
                }

                if (collapsed) break;
                start = end + 1;
            }
        }

        return lines;
    }

    // A small arrow: pointing right when collapsed, down when open
    void chevron(UIDrawList& ui, Vec2 center, bool open, const Color& color) {
        if (open) {
            ui.line(center + Vec2(-CHEVRON, -CHEVRON * 0.5f), center + Vec2(0.0f, CHEVRON * 0.5f), 1.5f, color);
            ui.line(center + Vec2(0.0f, CHEVRON * 0.5f), center + Vec2(CHEVRON, -CHEVRON * 0.5f), 1.5f, color);
        } else {
            ui.line(center + Vec2(-CHEVRON * 0.5f, -CHEVRON), center + Vec2(CHEVRON * 0.5f, 0.0f), 1.5f, color);
            ui.line(center + Vec2(CHEVRON * 0.5f, 0.0f), center + Vec2(-CHEVRON * 0.5f, CHEVRON), 1.5f, color);
        }
    }
}

void updateConsoleView(AppContext& ctx) {
    ConsoleViewState& view = ctx.consoleView;
    const InputState& input = ctx.systems.input;
    const Vec2 mouse(static_cast<f32>(input.getMouseX()), static_cast<f32>(input.getMouseY()));

    if (input.getScroll() != 0 && view.list.contains(mouse)) view.scrollRequest += input.getScroll() / WHEEL_DELTA_PER_LINE;

    // Clicking a multi-line entry's first row collapses or expands it
    if (input.wasMousePressedThisFrame(static_cast<u16>(MouseButton::Left))) {
        for (const ConsoleViewState::Toggle& toggle : view.toggles) {
            if (toggle.rect.contains(mouse)) {
                ctx.systems.console.toggleEntry(toggle.entry);
                view.anchorEntry = static_cast<i32>(toggle.entry);
                view.anchorRow = toggle.row;
                break;
            }
        }
    }
}

void drawConsole(AppContext& ctx, UIDrawList& ui, const Rect& viewport) {
    Console& console = ctx.systems.console;
    ConsoleViewState& view = ctx.consoleView;
    const UIFont font = makeUIFont(FontId::Console, ctx.fonts.get(FontId::Console));
    const UIFont small = makeUIFont(FontId::UI, ctx.fonts.get(FontId::UI));
    const Vec2 mouse(static_cast<f32>(ctx.systems.input.getMouseX()), static_cast<f32>(ctx.systems.input.getMouseY()));

    const f32 height = std::min(MAX_HEIGHT, std::floor(viewport.height * HEIGHT_FRACTION));
    const Rect panel { viewport.x + MARGIN, viewport.bottom() - MARGIN - height, viewport.width - MARGIN * 2.0f, height };

    // Panel: same look as the floating panel
    ui.shadow({ panel.x, panel.y + UIStyle::PANEL_SHADOW_OFFSET, panel.width, panel.height }, UIStyle::PANEL_RADIUS, UIStyle::PANEL_SHADOW_BLUR, UIStyle::PANEL_SHADOW);
    ui.roundedRect(panel, UIStyle::PANEL_RADIUS, UIStyle::PANEL_BACKGROUND, UIStyle::PANEL_BORDER, 1.0f);

    // Header: title on the left, key hints on the right
    const f32 headerTextY = std::round(panel.y + (HEADER_HEIGHT - small.glyphHeight) * 0.5f);
    ui.text(Vec2(panel.x + PADDING, headerTextY), "Console", small, UIStyle::ACCENT_GREEN);

    const std::string_view hints = "Enter run   Up/Down history   Click to fold   / close";
    ui.text(Vec2(panel.right() - PADDING - measureText(small, hints).x, headerTextY), hints, small, UIStyle::TEXT_DIM);
    ui.rect({ panel.x + 1.0f, panel.y + HEADER_HEIGHT, panel.width - 2.0f, 1.0f }, UIStyle::SEPARATOR);

    // Input line along the bottom
    const f32 inputHeight = font.glyphHeight + INPUT_PADDING_Y * 2.0f;
    const Rect input { panel.x + PADDING, panel.bottom() - PADDING - inputHeight, panel.width - PADDING * 2.0f, inputHeight };
    ui.roundedRect(input, UIStyle::CORNER_RADIUS, UIStyle::LIST_BACKGROUND, UIStyle::ACCENT_GREEN, 1.0f);

    const f32 textY = std::round(input.y + INPUT_PADDING_Y);
    const f32 promptWidth = font.glyphWidth * 2.0f;
    ui.text(Vec2(input.x + INPUT_PADDING_X, textY), ">", font, UIStyle::ACCENT_GREEN);

    // The command scrolls sideways so the caret stays in view
    const std::string& command = console.getCurrentCommand();
    const u32 cursor = std::min(console.getCursor(), static_cast<u32>(command.size()));
    const Rect textArea { input.x + INPUT_PADDING_X + promptWidth, input.y, input.width - INPUT_PADDING_X * 2.0f - promptWidth, input.height };
    const f32 caretX = static_cast<f32>(cursor) * font.glyphWidth;
    const f32 scroll = std::max(0.0f, caretX + CARET_WIDTH - textArea.width);

    ui.pushClip(textArea);
    ui.text(Vec2(textArea.x - scroll, textY), command, font, UIStyle::TEXT);

    // The caret stays solid while typing, then blinks
    static std::string lastCommand;
    static u32 lastCursor = 0;
    static f64 lastChange = 0.0;
    const f64 time = now();
    if (command != lastCommand || cursor != lastCursor) {
        lastCommand = command;
        lastCursor = cursor;
        lastChange = time;
    }

    if (std::fmod(time - lastChange, CARET_BLINK * 2.0) < CARET_BLINK) {
        ui.rect({ std::round(textArea.x - scroll + caretX), textY, CARET_WIDTH, font.glyphHeight }, UIStyle::ACCENT_GREEN);
    }
    ui.popClip();

    // The list above the input: commands, their output, and errors, newest at the bottom
    const Rect list { panel.x + PADDING, panel.y + HEADER_HEIGHT + 1.0f, panel.width - PADDING * 2.0f, input.y - (panel.y + HEADER_HEIGHT + 1.0f) - LINE_GAP };
    const std::vector<ConsoleEntry>& entries = console.getEntries();
    // Characters that fit between the prompt column and the scrollbar
    const f32 textWidth = list.width - SCROLLBAR_SPACE - INPUT_PADDING_X - promptWidth;
    const std::size_t wrapWidth = static_cast<std::size_t>(std::max(1.0f, textWidth / font.glyphWidth));
    const std::vector<DisplayLine> lines = layoutLines(entries, wrapWidth);
    const i32 count = static_cast<i32>(lines.size());
    const f32 lineHeight = font.glyphHeight + LINE_GAP;
    const i32 visibleLines = std::max(1, static_cast<i32>(list.height / lineHeight));
    const i32 newest = count - 1;
    view.list = list;

    // Scrolling: new entries and a fresh command go back to the newest line; the wheel and Up/Down move away from it
    if (entries.size() != view.seenEntries) {
        view.seenEntries = static_cast<u32>(entries.size());
        view.followNewest = true;
    }

    if (view.scrollRequest != 0) {
        view.bottomLine = std::min(view.bottomLine, newest) - view.scrollRequest;
        view.scrollRequest = 0;
        view.followNewest = view.bottomLine >= newest;
    }

    const i32 browsed = console.getBrowsedIndex();
    if (browsed != view.lastBrowsed) {
        view.lastBrowsed = browsed;
        view.followNewest = browsed < 0;

        // Only scroll as far as needed to keep the recalled command in view
        for (i32 line = 0; line < count && browsed >= 0; ++line) {
            if (!lines[line].first || entries[lines[line].entry].historyIndex != browsed) continue;
            if (line > view.bottomLine) view.bottomLine = line;
            if (line < view.bottomLine - visibleLines + 1) view.bottomLine = line + visibleLines - 1;
            break;
        }
    }

    // A clicked entry stays on its row, so opening it shows its lines below the click
    if (view.anchorEntry >= 0) {
        for (i32 line = 0; line < count; ++line) {
            if (static_cast<i32>(lines[line].entry) != view.anchorEntry) continue;
            view.bottomLine = line + view.anchorRow;
            view.followNewest = view.bottomLine >= newest;
            break;
        }
        view.anchorEntry = -1;
    }

    if (view.followNewest) view.bottomLine = newest;
    view.bottomLine = std::clamp(view.bottomLine, std::min(newest, visibleLines - 1), newest);

    view.toggles.clear();
    ui.pushClip(list);

    if (entries.empty()) {
        ui.text(Vec2(list.x, list.bottom() - lineHeight), "Type a command and press Enter (help lists them)", small, UIStyle::TEXT_DIM);
    }

    const f32 gutterX = list.x + INPUT_PADDING_X;
    const f32 textX = gutterX + promptWidth;

    for (i32 row = 0; row < visibleLines && view.bottomLine - row >= 0; ++row) {
        const i32 index = view.bottomLine - row;
        const DisplayLine& line = lines[index];
        const ConsoleEntry& entry = entries[line.entry];
        const f32 y = list.bottom() - static_cast<f32>(row + 1) * lineHeight;
        const Rect rowRect { list.x, y - LINE_GAP * 0.5f, list.width - SCROLLBAR_SPACE, lineHeight };

        const bool isCommand = entry.kind == ConsoleEntryKind::Command;
        const bool current = isCommand && browsed >= 0 && entry.historyIndex == browsed;
        const bool toggle = line.first && entry.collapsible();

        // The command Up/Down brought back stands out like a selected list row
        if (current) {
            ui.roundedRect(rowRect, UIStyle::CORNER_RADIUS, UIStyle::ACCENT_SOFT);
            ui.rect({ rowRect.x, rowRect.y, UIStyle::SELECTED_BAR_WIDTH, rowRect.height }, UIStyle::ACCENT);
        }

        if (toggle) {
            view.toggles.push_back({ rowRect, line.entry, row });
            if (rowRect.contains(mouse)) ui.roundedRect(rowRect, UIStyle::CORNER_RADIUS, UIStyle::ROW_HOVER);
        }

        Color textColor = OUTPUT_TEXT;
        if (isCommand) {
            const f32 age = static_cast<f32>(newest - index) / static_cast<f32>(std::max(visibleLines, 2) - 1);
            textColor = current ? UIStyle::TEXT : mix(HISTORY_NEWEST, HISTORY_OLDEST, std::min(age, 1.0f));
            ui.text(Vec2(gutterX, y), ">", font, current ? UIStyle::ACCENT_GREEN : PROMPT_DIM);
        } else if (entry.kind == ConsoleEntryKind::Error) {
            textColor = ERROR_TEXT;
            if (line.first && !toggle) ui.text(Vec2(gutterX, y), "!", font, ERROR_TEXT);
        }

        if (toggle) chevron(ui, Vec2(gutterX + font.glyphWidth * 0.5f, y + font.glyphHeight * 0.5f), entry.expanded, textColor);

        ui.text(Vec2(textX, y), line.text, font, textColor);

        if (line.hiddenLines > 0) {
            const std::string more = "  (+" + std::to_string(line.hiddenLines) + (line.hiddenLines == 1 ? " line)" : " lines)");
            ui.text(Vec2(textX + static_cast<f32>(line.text.size()) * font.glyphWidth, y), more, font, UIStyle::TEXT_DIM);
        }
    }

    ui.popClip();

    // A thin scrollbar when there's more than fits
    if (count > visibleLines) {
        const Rect track { list.right() - UIStyle::SCROLLBAR_WIDTH, list.y + LINE_GAP, UIStyle::SCROLLBAR_WIDTH, list.height - LINE_GAP * 2.0f };
        const f32 thumbHeight = std::max(UIStyle::SCROLLBAR_MIN_THUMB, track.height * static_cast<f32>(visibleLines) / static_cast<f32>(count));
        const f32 top = static_cast<f32>(view.bottomLine + 1 - visibleLines) / static_cast<f32>(count - visibleLines);

        ui.roundedRect(track, UIStyle::SCROLLBAR_WIDTH * 0.5f, UIStyle::SCROLLBAR_TRACK);
        ui.roundedRect({ track.x, track.y + (track.height - thumbHeight) * top, track.width, thumbHeight }, UIStyle::SCROLLBAR_WIDTH * 0.5f, UIStyle::SCROLLBAR_THUMB);
    }
}
