#pragma once

#include <vector>
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
