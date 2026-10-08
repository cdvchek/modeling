#pragma once

#include <filesystem>
#include <string>

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

    // The user's Documents folder (wherever Windows keeps it, e.g. in OneDrive); empty if it can't be found
    std::filesystem::path documentsFolder();

    enum class SaveChoice { Save, DontSave, Cancel };

    // "Save changes to name?" with Save, Don't Save, and Cancel
    SaveChoice askToSaveChanges(void* window, const std::string& title, const std::string& name);
}