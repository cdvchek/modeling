#include "platform/platform.hpp"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>

void Platform::pollEvents() {
    static MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void* Platform::getGLProcAddress(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (p == 0 || (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) || (p == (void*)-1)) {
        HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void *)GetProcAddress(module, name);
    }
    
    return p;
}
std::string Platform::getClipboardText() {
    std::string text;
    if (!OpenClipboard(nullptr)) return text;

    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(data))) {
            for (const wchar_t* c = wide; *c; ++c) text += *c < 128 ? static_cast<char>(*c) : '?';
            GlobalUnlock(data);
        }
    }

    CloseClipboard();
    return text;
}

void Platform::setClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    // The clipboard takes ownership of the memory once SetClipboardData succeeds
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
        wchar_t* wide = static_cast<wchar_t*>(GlobalLock(memory));
        for (std::size_t i = 0; i < text.size(); ++i) wide[i] = static_cast<unsigned char>(text[i]);
        wide[text.size()] = 0;
        GlobalUnlock(memory);

        if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    }

    CloseClipboard();
}

namespace {
    std::wstring widen(const std::string& text) {
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    // "Type name (*.ext)", "*.ext", then all files; each part ends in a null and the list in two
    std::wstring fileFilter(const std::string& typeName, const std::string& extension) {
        const std::wstring pattern = L"*" + widen(extension);
        std::wstring filter = widen(typeName) + L" (" + pattern + L")";
        filter += L'\0' + pattern + L'\0';
        filter += L"All files (*.*)";
        filter += L'\0';
        filter += L"*.*";
        filter += L'\0';
        return filter;
    }

    std::vector<std::filesystem::path> runFileDialog(void* window, const std::string& typeName, const std::string& extension, const std::filesystem::path& folderPath, const std::filesystem::path& fileName, bool save, bool multiple = false) {
        const std::wstring filter = fileFilter(typeName, extension);
        const std::wstring defaultExtension = widen(extension.size() > 1 ? extension.substr(1) : extension);

        std::wstring buffer = fileName.wstring();
        buffer.resize(32768, L'\0');

        const std::wstring folder = folderPath.wstring();

        OPENFILENAMEW dialog {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = static_cast<HWND>(window);
        dialog.lpstrFilter = filter.c_str();
        dialog.nFilterIndex = 1;
        dialog.lpstrFile = buffer.data();
        dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.lpstrInitialDir = folder.empty() ? nullptr : folder.c_str();
        dialog.lpstrDefExt = defaultExtension.c_str();
        // NOCHANGEDIR: otherwise the dialog moves the app's working directory
        dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR
                     | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST) | (multiple ? OFN_ALLOWMULTISELECT : 0);

        const BOOL chosen = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
        if (!chosen) return {};

        // Several files come back as the folder, then each name, each ending in a null, the list in two
        std::vector<std::filesystem::path> paths;
        const std::wstring first(buffer.c_str());
        const wchar_t* name = buffer.c_str() + first.size() + 1;
        if (!multiple || *name == L'\0') return { std::filesystem::path(first) };

        for (; *name != L'\0'; name += std::wcslen(name) + 1) paths.push_back(std::filesystem::path(first) / name);
        return paths;
    }
}

std::filesystem::path Platform::chooseOpenFile(void* window, const std::string& typeName, const std::string& extension, const std::filesystem::path& folder) {
    const std::vector<std::filesystem::path> paths = runFileDialog(window, typeName, extension, folder, {}, false);
    return paths.empty() ? std::filesystem::path() : paths.front();
}

std::vector<std::filesystem::path> Platform::chooseOpenFiles(void* window, const std::string& typeName, const std::string& extension, const std::filesystem::path& folder) {
    return runFileDialog(window, typeName, extension, folder, {}, false, true);
}

std::filesystem::path Platform::chooseSaveFile(void* window, const std::string& typeName, const std::string& extension, const std::filesystem::path& folder, const std::filesystem::path& fileName) {
    const std::vector<std::filesystem::path> paths = runFileDialog(window, typeName, extension, folder, fileName, true);
    return paths.empty() ? std::filesystem::path() : paths.front();
}

std::filesystem::path Platform::chooseFolder(void* window, const std::filesystem::path& folder) {
    // The folder picker is a COM object; initializing again on this thread is fine
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::filesystem::path result;

    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);

        IShellItem* start = nullptr;
        if (!folder.empty() && SUCCEEDED(SHCreateItemFromParsingName(folder.wstring().c_str(), nullptr, IID_PPV_ARGS(&start)))) {
            dialog->SetFolder(start);
            start->Release();
        }

        IShellItem* picked = nullptr;
        if (SUCCEEDED(dialog->Show(static_cast<HWND>(window))) && SUCCEEDED(dialog->GetResult(&picked))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(picked->GetDisplayName(SIGDN_FILESYSPATH, &path))) result = path;
            CoTaskMemFree(path);
            picked->Release();
        }
        dialog->Release();
    }

    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}

std::filesystem::path Platform::documentsFolder() {
    PWSTR path = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path))) result = path;
    CoTaskMemFree(path);
    return result;
}

