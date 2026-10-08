#pragma once

#include <functional>
#include <string>
#include <vector>
#include <types>

// What exporting chosen objects to a folder will write: each object's file name, whether it collides, and
// what happens then. Worked out again whenever the folder, the checkboxes, or a choice changes.

enum class ExportStatus : u8 {
    New,        // nothing with that name in the folder
    Exists,     // a file with that name is already there
    Duplicate   // an earlier checked object in this export has the same name
};

enum class CollisionChoice : u8 {
    Replace,
    Rename,     // the next free name: "Rock 2.vlmobj", then "Rock 3"...
    Skip
};

struct ExportPlanItem {
    // In
    std::string objectName;
    bool checked = false;
    CollisionChoice choice = CollisionChoice::Replace;

    // Out
    ExportStatus status = ExportStatus::New;
    std::string fileName;   // what will be written (or would be, for unchecked and skipped items)
    bool write = false;     // checked and not skipped
    bool renamed = false;   // fileName differs from the object's own
};

// "<name>.vlmobj", with characters Windows doesn't allow in file names (\ / : * ? " < > | and control
// characters) replaced by '_', and an empty name as "Untitled"
std::string assetFileName(const std::string& objectName);

// Fills in each item's status and file name, top to bottom. fileExists tells whether a file name is taken in the
// folder. Names are compared ignoring case, as Windows does. A duplicate always renames, since replacing would
// overwrite a file this same export writes.
void planExport(std::vector<ExportPlanItem>& items, const std::function<bool(const std::string& fileName)>& fileExists);
