#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <types>

#include "scene/scene.hpp"
#include "input/contexts.hpp"
#include "ui/ui_types.hpp"

// Valuma Studio project files (.vlm): the scene plus the editor state needed to carry on where you left off.
// Layout (all little-endian): a 16-byte header, a directory with one entry per chunk, then the chunks.
// Each object is its own chunk, so objects are written and read in parallel; unknown chunk types are skipped.
// Each reference image is its own chunk too, carrying its PNG file unchanged.
namespace ProjectFile {
    inline constexpr const char* EXTENSION = ".vlm";
    inline constexpr u32 FORMAT_VERSION = 1;

    // Editor state saved alongside the scene
    struct View {
        u32 selectionMode = InputContext_SelectionVertex;   // one InputContext selection flag
        u32 lastEditMode = InputContext_SelectionVertex;    // where Tab returns to from object mode
        bool debug = false;

        bool headlightEnabled = true;
        Vec3 headlightColor { 1.0f, 1.0f, 1.0f };
        f32 headlightStrength = 0.8f;

        Vec3 backFaceTint { 1.0f, 1.0f, 1.0f };

        bool showPanel = true;
        Rect panelRect;
        i32 panelTab = 0;

        bool showOrigins = true;

        // Stops; 0 leaves lighting as it is
        f32 exposure = 0.0f;
        // Material view; off is clay view
        bool showMaterials = true;
        bool showUVChecker = false;
        // The workspace (0 Model, 1 UV, 2 Paint) and the UV workspace's split, as a fraction of the width
        u8 workspace = 0;
        f32 uvSplit = 0.5f;
        // The UV editor's view (zoom 0: frame everything when first shown) and whether its grid shows
        f32 uvCenter[2] = { 0.5f, 0.5f };
        f32 uvZoom = 0.0f;
        bool uvGrid = true;
        // Paint: 2D (the flat texture) or 3D, and the 2D view (zoom 0: frame the texture when first shown)
        bool paint2D = false;
        f32 paintCenter[2] = { 0.5f, 0.5f };
        f32 paintZoom = 0.0f;
        // Paint's brush: color, size in texture pixels, softness, opacity, spacing as a part of the size, and whether it's the eraser
        Vec3 brushColor { 0.0f, 0.0f, 0.0f };
        f32 brushSize = 24.0f;
        f32 brushSoftness = 0.5f;
        f32 brushOpacity = 1.0f;
        f32 brushSpacing = 0.1f;
        bool brushErase = false;

        // Where assets were last exported, as storeFolder gives it (relative to the project file when nearby);
        // empty means the default, an Exports folder next to the project file
        std::string exportFolder;
    };

    // The whole file in memory
    std::vector<u8> write(const Scene& scene, const View& view);

    // Fills an empty scene (objects, lights, reference images, camera, active object) and view; on failure says why in error
    bool read(const std::vector<u8>& bytes, Scene& scene, View& view, std::string& error);

    // Writes to a temporary file next to path, then swaps it in, so a failed save never damages the old file
    bool save(const std::filesystem::path& path, const Scene& scene, const View& view, std::string& error);
    bool load(const std::filesystem::path& path, Scene& scene, View& view, std::string& error);

    // Lists the header and every chunk (type, size, checksum, contents in brief), for debugging a file
    std::string describe(const std::vector<u8>& bytes);

    bool readFile(const std::filesystem::path& path, std::vector<u8>& bytes, std::string& error);

    // A folder as a project saved at projectFile stores it: relative to the project's folder when the folder is
    // inside it or next to it (one level up at most), so moving the project along with it keeps it working;
    // otherwise the full path. UTF-8 with forward slashes; empty stays empty.
    std::string storeFolder(const std::filesystem::path& folder, const std::filesystem::path& projectFile);
    // Back to a full path, for a project now at projectFile
    std::filesystem::path resolveFolder(const std::string& stored, const std::filesystem::path& projectFile);
}
