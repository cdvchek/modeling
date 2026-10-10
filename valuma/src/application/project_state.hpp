#pragma once

#include <filesystem>
#include <string>
#include <types>

#include "project/project_file.hpp"

// Which file the scene belongs to and whether it changed since it was saved
struct ProjectState {
    std::filesystem::path path;       // empty until the project is saved or opened
    u64 savedState = 0;               // History::stateId() when last saved, opened, or started new
    ProjectFile::View startingView;   // the view a new project starts with
    std::filesystem::path exportFolder;  // where assets were last exported; empty until then (see exportFolder())
    std::string shownTitle;           // the window title last set, so it's only set again when it changes
};
