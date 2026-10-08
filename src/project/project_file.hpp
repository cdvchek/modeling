#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <types>

#include "scene/scene.hpp"
#include "core/input/contexts.hpp"
#include "ui/ui_types.hpp"

// Valuma Studio project files (.vlm): the scene plus the editor state needed to carry on where you left off.
// Layout (all little-endian): a 16-byte header, a directory with one entry per chunk, then the chunks.
// Each object is its own chunk, so objects are written and read in parallel; unknown chunk types are skipped.
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
        f32 headlightStrength = 0.4f;

        Vec3 backFaceTint { 1.0f, 1.0f, 1.0f };

        bool showPanel = true;
        Rect panelRect;
        i32 panelTab = 0;
    };

    // The whole file in memory
    std::vector<u8> write(const Scene& scene, const View& view);

    // Fills an empty scene (objects, lights, camera, active object) and view; on failure says why in error
    bool read(const std::vector<u8>& bytes, Scene& scene, View& view, std::string& error);

    // Writes to a temporary file next to path, then swaps it in, so a failed save never damages the old file
    bool save(const std::filesystem::path& path, const Scene& scene, const View& view, std::string& error);
    bool load(const std::filesystem::path& path, Scene& scene, View& view, std::string& error);

    // Lists the header and every chunk (type, size, checksum, contents in brief), for debugging a file
    std::string describe(const std::vector<u8>& bytes);

    bool readFile(const std::filesystem::path& path, std::vector<u8>& bytes, std::string& error);
}
