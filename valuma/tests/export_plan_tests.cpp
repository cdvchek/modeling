#include "test.hpp"
#include "asset/export_plan.hpp"

#include <set>

namespace {
    ExportPlanItem item(const std::string& name, bool checked = true, CollisionChoice choice = CollisionChoice::Replace) {
        ExportPlanItem result;
        result.objectName = name;
        result.checked = checked;
        result.choice = choice;
        return result;
    }

    // A folder holding these files (Windows ignores case)
    std::function<bool(const std::string&)> folderWith(std::set<std::string> files) {
        return [files](const std::string& name) {
            std::string lower = name;
            for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (std::string file : files) {
                for (char& c : file) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (file == lower) return true;
            }
            return false;
        };
    }
}

TEST_CASE(export_file_names_are_safe) {
    CHECK(assetFileName("Rock") == "Rock.vlmobj");
    CHECK(assetFileName("Tree trunk") == "Tree trunk.vlmobj");
    CHECK(assetFileName("a/b\\c:d*e?f\"g<h>i|j") == "a_b_c_d_e_f_g_h_i_j.vlmobj");
    CHECK(assetFileName("name. ") == "name.vlmobj");
    CHECK(assetFileName("") == "Untitled.vlmobj");
}

TEST_CASE(export_plan_new_exists_and_choices) {
    std::vector<ExportPlanItem> items = {
        item("Rock"),
        item("Tree"),
        item("Bush", true, CollisionChoice::Rename),
        item("Stump", true, CollisionChoice::Skip),
        item("Log", false),
    };
    planExport(items, folderWith({ "tree.vlmobj", "Bush.vlmobj", "Bush 2.vlmobj", "Stump.vlmobj", "Log.vlmobj" }));

    CHECK(items[0].status == ExportStatus::New && items[0].write && items[0].fileName == "Rock.vlmobj");

    // Replace (the default) keeps the name; case doesn't matter, as on Windows
    CHECK(items[1].status == ExportStatus::Exists && items[1].write && !items[1].renamed && items[1].fileName == "Tree.vlmobj");

    // Rename skips names already in the folder
    CHECK(items[2].status == ExportStatus::Exists && items[2].renamed && items[2].fileName == "Bush 3.vlmobj");

    CHECK(items[3].status == ExportStatus::Exists && !items[3].write);

    // Unchecked rows still show their status but write nothing
    CHECK(items[4].status == ExportStatus::Exists && !items[4].write);
}

TEST_CASE(export_plan_duplicates_always_rename) {
    std::vector<ExportPlanItem> items = {
        item("Rock"),
        item("Rock", true, CollisionChoice::Replace),
        item("rock", true, CollisionChoice::Skip),
        item("Rock", false),
        item("Rock 2"),
    };
    planExport(items, folderWith({}));

    CHECK(items[0].status == ExportStatus::New && items[0].fileName == "Rock.vlmobj");
    CHECK(items[1].status == ExportStatus::Duplicate && items[1].renamed && items[1].fileName == "Rock 2.vlmobj" && items[1].write);
    // Skip doesn't apply to duplicates: the earlier file is this export's own
    CHECK(items[2].status == ExportStatus::Duplicate && items[2].fileName == "rock 3.vlmobj" && items[2].write);
    // Unchecked rows don't claim a name
    CHECK(items[3].status == ExportStatus::New && !items[3].write);
    // An object really named "Rock 2" finds its name taken by a rename above it
    CHECK(items[4].status == ExportStatus::Duplicate && items[4].fileName == "Rock 2 2.vlmobj");
}

TEST_CASE(export_plan_skipped_rows_free_their_name) {
    // A skipped file isn't written, so a later object with the same name doesn't count as a duplicate of it
    std::vector<ExportPlanItem> items = { item("Rock", true, CollisionChoice::Skip), item("Rock") };
    planExport(items, folderWith({ "Rock.vlmobj" }));

    CHECK(!items[0].write);
    CHECK(items[1].status == ExportStatus::Exists && items[1].write && items[1].fileName == "Rock.vlmobj");
}
