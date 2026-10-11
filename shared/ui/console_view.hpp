#pragma once

#include <vector>
#include "core/console/console.hpp"
#include "core/font/font_library.hpp"
#include "core/input/input_state.hpp"
#include "ui/ui_draw_list.hpp"
#include "ui/ui_types.hpp"
// What the console view keeps between frames: its scroll and where it drew clickable rows
struct ConsoleViewState {
    i32 bottomLine = 0;          // the display line shown at the bottom of the list
    bool followNewest = true;    // keep the newest line at the bottom as entries arrive
    i32 scrollRequest = 0;       // lines from the wheel (positive is older), applied at the next draw
    u32 seenEntries = 0;         // entry count at the last draw; new entries jump back to the newest
    i32 lastBrowsed = -1;        // Up/Down position at the last draw
    Rect list;                   // where the list was drawn

    // Rows that collapse or expand an entry: where, which entry, and how many rows up from the bottom
    struct Toggle {
        Rect rect;
        u32 entry;
        i32 row;
    };
    std::vector<Toggle> toggles;

    // A clicked entry keeps its row in place after it opens or closes; -1 when nothing was clicked
    i32 anchorEntry = -1;
    i32 anchorRow = 0;
};

// The console overlay: a panel docked at the bottom of viewport with commands, output, and errors above an input line
void drawConsole(Console& console, ConsoleViewState& view, const FontLibrary& fonts, const InputState& inputState, UIDrawList& ui, const Rect& viewport);

// Mouse wheel scrolling and clicks that collapse or expand entries; run it while the console is open
void updateConsoleView(Console& console, ConsoleViewState& view, const InputState& input);
