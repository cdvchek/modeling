#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Platform {
    void pollEvents();
    void* getGLProcAddress(const char* name);

    // Plain text on the system clipboard; characters outside ASCII come back as '?'
    std::string getClipboardText();
    void setClipboardText(const std::string& text);

    // Native Open and Save As dialogs over window, listing files with extension (like ".vlm") under typeName.
    // They start in folder (Windows may use a folder it remembers instead). Save starts with fileName
    // and adds the extension when it's left off. Both return an empty path when cancelled.
    std::filesystem::path chooseOpenFile(void* window, const std::string& typeName, const std::string& extension, const std::filesystem::path& folder);
    std::filesystem::path chooseSaveFile(void* window, const std::string& typeName, const std::string& extension, const std::filesystem::path& folder, const std::filesystem::path& fileName);
    // Open with several files allowed; empty when cancelled
    std::vector<std::filesystem::path> chooseOpenFiles(void* window, const std::string& typeName, const std::string& extension, const std::filesystem::path& folder);
    // Windows' folder picker, starting in folder; empty when cancelled
    std::filesystem::path chooseFolder(void* window, const std::filesystem::path& folder);

    // The user's Documents folder (wherever Windows keeps it, e.g. in OneDrive); empty if it can't be found
    std::filesystem::path documentsFolder();
}