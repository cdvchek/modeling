#pragma once

#include <types>
#include <filesystem>
#include <string>
#include <string_view>

// An Aevora project: a folder holding the project file (.aev) with its scenes, data, and assets beside it
struct Project {
    std::string name;
    std::filesystem::path file;

    std::filesystem::path folder() const { return file.parent_path(); }
};

// The project file is text: a line naming the format and version, then one key = "value" per line
namespace ProjectFile {
    inline constexpr const char* EXTENSION = ".aev";
    inline constexpr u32 VERSION = 1;
    inline constexpr const char* FOLDERS[] = { "scenes", "data", "assets" };

    std::string write(const Project& project);
    bool read(std::string_view text, Project& project, std::string& error);

    bool save(const Project& project, std::string& error);
    bool load(const std::filesystem::path& file, Project& project, std::string& error);

    // Makes the folder, its project folders, and a project file named after it; refuses a folder that has one
    bool create(const std::filesystem::path& folder, Project& project, std::string& error);

    // The project file in folder, or an empty path
    std::filesystem::path find(const std::filesystem::path& folder);
}
