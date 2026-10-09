#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <types>

#include "asset/export_plan.hpp"
#include "scene/objects/object_collection.hpp"

// At most one modal window is open at a time; it takes all input until it closes
enum class ModalKind : u8 {
    None,
    Prompt,
    Export
};

// A question with a row of buttons. The answer is handled in checkActions on the next frame, not while drawing.
struct PromptState {
    std::string title;
    std::string message;
    std::vector<std::string> buttons;
    i32 confirmButton = 0;     // Enter, and drawn as the default
    i32 cancelButton = -1;     // Escape
    std::function<void(i32 button)> onChoice;
    i32 chosen = -1;           // set by a click; acted on next frame
};

struct ExportRow {
    ObjectHandle object;
    ExportPlanItem item;
};

struct ExportWindowState {
    std::filesystem::path folder;
    std::vector<ExportRow> rows;
    i32 allChoice = 0;         // the header switch: 0 Replace, 1 Rename, 2 Skip; -1 when rows differ

    // Whether file names exist in the folder, checked at most every FILE_CHECK_INTERVAL seconds
    std::unordered_map<std::string, bool> existing;
    f64 checkedAt = -1.0;

    // Set by clicks while drawing, acted on next frame
    bool exportRequested = false;
    bool browseRequested = false;
    bool closeRequested = false;
};

struct ModalState {
    ModalKind kind = ModalKind::None;
    PromptState prompt;
    ExportWindowState exportWindow;
};
