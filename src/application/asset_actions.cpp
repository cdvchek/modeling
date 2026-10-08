#include "application/asset_actions.hpp"
#include "application/modal_windows.hpp"
#include "application/object_commands.hpp"
#include "application/project_actions.hpp"
#include "asset/asset_file.hpp"
#include "platform/platform.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>

namespace {
    constexpr const char* ASSET_TYPE = "Valuma Studio asset";
    constexpr const char* EXPORTS_FOLDER = "Exports";

    constexpr f32 WINDOW_WIDTH = 760.0f;
    constexpr f32 WINDOW_HEIGHT = 540.0f;
    constexpr f32 WINDOW_MARGIN = 40.0f;
    constexpr f32 FOLDER_LABEL_WIDTH = 64.0f;
    constexpr f32 BROWSE_WIDTH = 96.0f;
    constexpr f32 CHECK_COLUMN = 26.0f;
    constexpr f32 COLUMN_GAP = 10.0f;
    constexpr f32 BUTTON_WIDTH = 110.0f;
    constexpr f32 CHILD_INDENT = 16.0f;

    // How long a check of which files exist in the folder is trusted before looking again
    constexpr f64 FILE_CHECK_INTERVAL = 1.0;

    const std::vector<std::string_view> CHOICES = { "Replace", "Rename", "Skip" };

    std::string utf8(const std::filesystem::path& path) {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    std::filesystem::path fromUtf8(const std::string& text) {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }

    f64 now() {
        return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // Column rects across a row: checkbox, object, file, status, choice
    struct Columns {
        Rect check, name, file, status, choice;
    };

    Columns columns(const Rect& row) {
        const f32 rest = row.width - CHECK_COLUMN;
        Columns c;
        c.check = { row.x, row.y, CHECK_COLUMN, row.height };
        c.name = { c.check.right(), row.y, std::floor(rest * 0.24f) - COLUMN_GAP, row.height };
        c.file = { c.name.right() + COLUMN_GAP, row.y, std::floor(rest * 0.31f) - COLUMN_GAP, row.height };
        c.status = { c.file.right() + COLUMN_GAP, row.y, std::floor(rest * 0.16f) - COLUMN_GAP, row.height };
        c.choice = { c.status.right() + COLUMN_GAP, row.y, row.right() - c.status.right() - COLUMN_GAP, row.height };
        return c;
    }

    // An object and everything under it, parents first, depth counted from it
    std::vector<HierarchyEntry> familyOf(const ObjectCollection& objects, ObjectHandle root) {
        std::vector<HierarchyEntry> family;
        bool inside = false;
        u32 rootDepth = 0;
        for (const HierarchyEntry& entry : objects.hierarchy()) {
            if (entry.handle == root) {
                inside = true;
                rootDepth = entry.depth;
            } else if (inside && entry.depth <= rootDepth) {
                break;
            }
            if (inside) family.push_back({ entry.handle, entry.depth - rootDepth });
        }
        return family;
    }

    // Shortened with "..." when it doesn't fit its column
    void clippedText(UIContext& ui, const Rect& rect, std::string_view text, const Color& color) {
        ui.text(rect, fitText(ui.font(), text, rect.width), color);
    }

    // Keeps the end of a long path, which is the part that tells folders apart
    std::string fitStart(const UIFont& font, const std::string& text, f32 width) {
        if (measureText(font, text).x <= width) return text;
        const std::size_t keep = static_cast<std::size_t>(std::max(0.0f, width / font.glyphWidth - 3.0f));
        return keep < text.size() ? "..." + text.substr(text.size() - keep) : text;
    }

    // Windows finds "torus.vlmobj" when the file is "Torus.vlmobj"; the object should get the name as it's written
    std::filesystem::path nameOnDisk(const std::filesystem::path& path) {
        std::error_code code;
        const std::wstring wanted = path.filename().wstring();
        for (const auto& entry : std::filesystem::directory_iterator(path.parent_path(), code)) {
            const std::wstring name = entry.path().filename().wstring();
            if (name.size() == wanted.size() && std::equal(name.begin(), name.end(), wanted.begin(), [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); })) {
                return entry.path();
            }
        }
        return path;
    }

    // Works out every row's file name and status again, looking in the folder at most once a second
    void replan(ExportWindowState& state) {
        if (now() - state.checkedAt > FILE_CHECK_INTERVAL) {
            state.existing.clear();
            state.checkedAt = now();
        }

        std::vector<ExportPlanItem> items;
        for (const ExportRow& row : state.rows) items.push_back(row.item);

        planExport(items, [&state](const std::string& fileName) {
            auto found = state.existing.find(fileName);
            if (found != state.existing.end()) return found->second;
            std::error_code code;
            const bool exists = std::filesystem::exists(state.folder / fromUtf8(fileName), code);
            state.existing.emplace(fileName, exists);
            return exists;
        });

        for (std::size_t i = 0; i < items.size(); ++i) state.rows[i].item = items[i];
    }

    u32 writeCount(const ExportWindowState& state) {
        u32 count = 0;
        for (const ExportRow& row : state.rows) count += row.item.write ? 1 : 0;
        return count;
    }

    bool collides(const ExportPlanItem& item) {
        return item.checked && item.status == ExportStatus::Exists;
    }

    // The header switch shows the choice when every colliding row agrees, otherwise nothing
    i32 sharedChoice(const ExportWindowState& state) {
        i32 shared = -2;
        for (const ExportRow& row : state.rows) {
            if (!collides(row.item)) continue;
            const i32 choice = static_cast<i32>(row.item.choice);
            if (shared == -2) shared = choice;
            else if (shared != choice) return -1;
        }
        return shared == -2 ? -1 : shared;
    }

    void runExport(AppContext& ctx) {
        ExportWindowState& state = ctx.modal.exportWindow;
        state.checkedAt = -1.0;
        replan(state);

        std::error_code code;
        std::filesystem::create_directories(state.folder, code);
        if (code) {
            ctx.systems.console.printError("Couldn't create the export folder " + utf8(state.folder) + ": " + code.message());
            return;
        }

        std::string written, skipped;
        u32 count = 0;
        for (const ExportRow& row : state.rows) {
            const ExportPlanItem& item = row.item;
            if (item.checked && !item.write) skipped += (skipped.empty() ? "" : ", ") + item.objectName;
            if (!item.write) continue;

            const Object* object = ctx.scene.objects.tryGet(row.object);
            if (!object) continue;

            std::string error;
            if (!AssetFile::save(state.folder / fromUtf8(item.fileName), ctx.scene.objects, row.object, error)) {
                ctx.systems.console.printError("Couldn't export " + item.objectName + ": " + error);
                continue;
            }

            const char* note = item.renamed ? " (renamed)" : item.status == ExportStatus::Exists ? " (replaced)" : "";
            written += (written.empty() ? "" : ", ") + item.fileName + note;
            ++count;
        }

        // The folder is remembered even after a partial failure: it's where you chose to export
        ctx.project.exportFolder = state.folder;
        closeModal(ctx);

        if (count == 0 && skipped.empty()) return;
        std::string summary = "Exported " + std::to_string(count) + (count == 1 ? " file" : " files") + " to " + utf8(state.folder);
        if (!written.empty()) summary += ": " + written;
        if (!skipped.empty()) summary += "; skipped " + skipped;
        ctx.systems.console.print(summary);
    }
}

std::filesystem::path exportFolder(const AppContext& ctx) {
    if (!ctx.project.exportFolder.empty()) return ctx.project.exportFolder;
    const std::filesystem::path base = ctx.project.path.empty() ? projectsFolder() : ctx.project.path.parent_path();
    return base / EXPORTS_FOLDER;
}

void openExportWindow(AppContext& ctx) {
    ExportWindowState& state = ctx.modal.exportWindow;
    state = {};
    state.folder = exportFolder(ctx);

    const Selection& selection = ctx.scene.selection;
    const bool objectMode = ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionObject;

    // One row per top-level object, since a file holds an object and all its children; a selected child checks its family
    const ObjectCollection& objects = ctx.scene.objects;
    for (ObjectHandle handle : objects.handles()) {
        if (!objects.parentOf(handle).isNull()) continue;

        ExportRow row;
        row.object = handle;
        row.item.objectName = objects.get(handle).name;
        if (objectMode) {
            for (ObjectHandle selected : selection.getObjects()) row.item.checked |= objects.topLevelOf(selected) == handle;
        } else {
            row.item.checked = objects.topLevelOf(selection.getActiveObject()) == handle;
        }
        state.rows.push_back(row);
    }

    replan(state);
    openModal(ctx, ModalKind::Export);
}

void confirmExportWindow(AppContext& ctx) {
    if (writeCount(ctx.modal.exportWindow) > 0) ctx.modal.exportWindow.exportRequested = true;
}

void updateExportWindow(AppContext& ctx) {
    ExportWindowState& state = ctx.modal.exportWindow;

    if (state.closeRequested) {
        state.closeRequested = false;
        closeModal(ctx);
        return;
    }

    if (state.browseRequested) {
        state.browseRequested = false;
        const std::filesystem::path picked = Platform::chooseFolder(nativeWindow(ctx), state.folder);
        afterDialog(ctx);
        if (!picked.empty() && picked != state.folder) {
            state.folder = picked;
            state.checkedAt = -1.0;
        }
    }

    if (state.exportRequested) {
        state.exportRequested = false;
        runExport(ctx);
    }
}

void drawExportWindow(AppContext& ctx, const Rect& viewport) {
    UIContext& ui = ctx.ui;
    ExportWindowState& state = ctx.modal.exportWindow;
    const UIFont& font = ui.font();

    replan(state);

    const f32 width = std::min(WINDOW_WIDTH, viewport.width - WINDOW_MARGIN);
    const f32 height = std::min(WINDOW_HEIGHT, viewport.height - WINDOW_MARGIN);
    ui.beginModal("export", viewport, width, height, "Export");

    // Folder: label, path (the end of it when long), Browse
    const Rect folderRow = ui.row();
    ui.text({ folderRow.x, folderRow.y, FOLDER_LABEL_WIDTH, folderRow.height }, "Folder", UIStyle::TEXT_DIM);
    const Rect pathBox { folderRow.x + FOLDER_LABEL_WIDTH, folderRow.y, folderRow.width - FOLDER_LABEL_WIDTH - BROWSE_WIDTH - COLUMN_GAP, folderRow.height };
    ui.drawList().roundedRect(pathBox, UIStyle::CORNER_RADIUS, UIStyle::LIST_BACKGROUND, UIStyle::FRAME_BORDER, 1.0f);
    const Rect pathText { pathBox.x + UIStyle::TEXT_PADDING, pathBox.y, pathBox.width - UIStyle::TEXT_PADDING * 2.0f, pathBox.height };
    clippedText(ui, pathText, fitStart(font, utf8(state.folder), pathText.width), UIStyle::TEXT);
    if (ui.button("Browse...", { pathBox.right() + COLUMN_GAP, folderRow.y, BROWSE_WIDTH, folderRow.height })) state.browseRequested = true;

    ui.spacing();

    // Header: check everything, column titles, and one switch for every colliding row
    // Inset like the rows inside the list box below (its padding and scrollbar), so the columns line up
    const Rect headRow = ui.row();
    const f32 pad = UIStyle::CHILD_PADDING;
    const Columns head = columns({ headRow.x + pad, headRow.y, headRow.width - pad * 3.0f - UIStyle::SCROLLBAR_WIDTH, headRow.height });
    bool all = !state.rows.empty() && std::all_of(state.rows.begin(), state.rows.end(), [](const ExportRow& row) { return row.item.checked; });
    if (ui.checkbox("all", head.check, all)) {
        for (ExportRow& row : state.rows) row.item.checked = all;
    }
    ui.text(head.name, "Object", UIStyle::TEXT_DIM);
    ui.text(head.file, "File", UIStyle::TEXT_DIM);
    ui.text(head.status, "Status", UIStyle::TEXT_DIM);
    i32 allChoice = sharedChoice(state);
    if (ui.segmented("all choice", head.choice, allChoice, CHOICES)) {
        for (ExportRow& row : state.rows) {
            if (collides(row.item)) row.item.choice = static_cast<CollisionChoice>(allChoice);
        }
    }

    // One row per object, scrolling when there are many
    const f32 footer = UIStyle::ROW_HEIGHT + UIStyle::SECTION_SPACING + UIStyle::ITEM_SPACING;
    const f32 used = (UIStyle::ROW_HEIGHT + UIStyle::ITEM_SPACING) * 2.0f + UIStyle::SECTION_SPACING;
    const f32 listHeight = height - UIStyle::PANEL_HEADER_HEIGHT - UIStyle::PADDING * 2.0f - used - footer;
    ui.beginChild("objects", std::max(UIStyle::ROW_HEIGHT * 2.0f, listHeight));

    for (u32 i = 0; i < state.rows.size(); ++i) {
        ExportPlanItem& item = state.rows[i].item;
        ui.pushId(i);
        const Columns c = columns(ui.row());

        ui.checkbox("check", c.check, item.checked);
        clippedText(ui, c.name, item.objectName, item.checked ? UIStyle::TEXT : UIStyle::TEXT_DIM);
        const bool writes = item.write;
        clippedText(ui, c.file, item.fileName, writes ? (item.renamed ? UIStyle::ACCENT : UIStyle::TEXT) : UIStyle::TEXT_DIM);

        const char* status = item.status == ExportStatus::New ? "new" : item.status == ExportStatus::Exists ? "exists" : "duplicate";
        const Color statusColor = !item.checked ? UIStyle::TEXT_DIM : item.status == ExportStatus::New ? UIStyle::ACCENT_GREEN : UIStyle::WARNING;
        clippedText(ui, c.status, status, statusColor);

        if (collides(item)) {
            i32 choice = static_cast<i32>(item.choice);
            if (ui.segmented("choice", c.choice, choice, CHOICES)) item.choice = static_cast<CollisionChoice>(choice);
        } else if (item.checked && item.status == ExportStatus::Duplicate) {
            clippedText(ui, { c.choice.x + UIStyle::TEXT_PADDING, c.choice.y, c.choice.width, c.choice.height }, "Rename", UIStyle::TEXT_DIM);
        }

        // Its children go in the same file: listed greyed underneath, indented by level
        const std::vector<HierarchyEntry> family = familyOf(ctx.scene.objects, state.rows[i].object);
        for (std::size_t k = 1; k < family.size(); ++k) {
            const Rect child = ui.row();
            const Columns cc = columns(child);
            const f32 indent = CHECK_COLUMN + CHILD_INDENT * static_cast<f32>(family[k].depth);
            clippedText(ui, { child.x + indent, child.y, cc.file.x - child.x - indent - COLUMN_GAP, child.height },
                        ctx.scene.objects.get(family[k].handle).name, UIStyle::TEXT_DIM);
            clippedText(ui, cc.file, "in " + item.fileName, UIStyle::TEXT_DIM);
        }
        ui.popId();
    }
    if (state.rows.empty()) ui.label("There are no objects to export.", true);
    ui.endChild();

    ui.spacing();

    // Footer: what will happen, then Cancel and Export N
    const Rect footerRow = ui.row();
    const u32 count = writeCount(state);
    u32 replaced = 0, renamed = 0;
    for (const ExportRow& row : state.rows) {
        if (!row.item.write) continue;
        if (row.item.renamed) ++renamed;
        else if (row.item.status == ExportStatus::Exists) ++replaced;
    }
    std::string summary = count == 0 ? "Check the objects to export" : std::to_string(count) + (count == 1 ? " file" : " files");
    if (replaced > 0) summary += ", " + std::to_string(replaced) + " replacing existing";
    if (renamed > 0) summary += ", " + std::to_string(renamed) + " renamed";
    ui.text({ footerRow.x, footerRow.y, footerRow.width - BUTTON_WIDTH * 2.0f - COLUMN_GAP * 2.0f, footerRow.height }, summary, UIStyle::TEXT_DIM);

    const Rect exportRect { footerRow.right() - BUTTON_WIDTH, footerRow.y, BUTTON_WIDTH, footerRow.height };
    const Rect cancelRect { exportRect.x - COLUMN_GAP - BUTTON_WIDTH, footerRow.y, BUTTON_WIDTH, footerRow.height };
    if (ui.button("Cancel", cancelRect)) state.closeRequested = true;
    if (ui.button("Export " + std::to_string(count), exportRect, count > 0)) state.exportRequested = true;
    if (count > 0) ui.drawList().roundedRect(exportRect, UIStyle::CORNER_RADIUS, { 0.0f, 0.0f, 0.0f, 0.0f }, UIStyle::ACCENT, 1.0f);

    ui.endModal();
}

void importAssets(AppContext& ctx) {
    std::error_code code;
    std::filesystem::path folder = exportFolder(ctx);
    if (!std::filesystem::is_directory(folder, code)) folder = projectsFolder();

    const std::vector<std::filesystem::path> paths = Platform::chooseOpenFiles(nativeWindow(ctx), ASSET_TYPE, AssetFile::EXTENSION, folder);
    afterDialog(ctx);

    for (const std::filesystem::path& path : paths) importAssetFrom(ctx, path);
}

bool importAssetFrom(AppContext& ctx, const std::filesystem::path& typed) {
    const std::filesystem::path path = nameOnDisk(typed);
    const std::string fileName = utf8(path.filename());

    std::vector<AssetFile::ImportedObject> imported;
    std::string error;
    if (!AssetFile::load(path, imported, error)) {
        ctx.systems.console.printError("Couldn't import " + fileName + ": " + error);
        return false;
    }

    ObjectCollection& objects = ctx.scene.objects;
    ctx.history.begin(ctx.scene);

    // The root is named after the file, like a preset is named after its type, and placed like one
    const std::string wanted = utf8(path.stem());
    Object& root = imported[0].object;
    root.name = objects.uniqueName(wanted.empty() ? root.name : wanted);
    const std::string name = root.name;

    // Its children keep their transforms relative to their parents, so they're linked as they are
    std::vector<ObjectHandle> handles;
    handles.push_back(placeNewObject(ctx, std::move(root)));
    for (std::size_t i = 1; i < imported.size(); ++i) {
        imported[i].object.name = objects.uniqueName(imported[i].object.name);
        imported[i].object.parent = imported[i].parent < handles.size() ? handles[imported[i].parent] : handles[0];
        handles.push_back(objects.add(std::move(imported[i].object)));
    }
    ctx.history.commit();

    const std::string parts = handles.size() > 1 ? " with " + std::to_string(handles.size() - 1) + (handles.size() == 2 ? " child" : " children") : "";
    ctx.systems.console.print("Imported " + fileName + (name == wanted ? "" : " as " + name) + parts);
    return true;
}
