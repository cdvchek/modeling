#include "engine/project/project.hpp"

#include <fstream>
#include <sstream>

namespace {
    constexpr std::string_view MAGIC = "aevora project";

    std::string_view trim(std::string_view text) {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) text.remove_prefix(1);
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
        return text;
    }

    std::string quote(std::string_view text) {
        std::string result = "\"";
        for (char character : text) {
            if (character == '"' || character == '\\') result += '\\';
            result += character;
        }
        return result + "\"";
    }

    // The text between the quotes; false for a value that isn't a quoted string
    bool unquote(std::string_view value, std::string& out) {
        if (value.size() < 2 || value.front() != '"' || value.back() != '"') return false;
        value = value.substr(1, value.size() - 2);

        out.clear();
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '\\') {
                if (++i == value.size()) return false;
            } else if (value[i] == '"') {
                return false;
            }
            out += value[i];
        }
        return true;
    }

    std::string pathText(const std::filesystem::path& path) {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }
}

std::string ProjectFile::write(const Project& project) {
    std::string text;
    text += std::string(MAGIC) + " " + std::to_string(VERSION) + "\n";
    text += "name = " + quote(project.name) + "\n";
    return text;
}

bool ProjectFile::read(std::string_view text, Project& project, std::string& error) {
    // 1. The first line says what this is and which version wrote it
    const std::size_t firstEnd = text.find('\n');
    const std::string_view first = trim(text.substr(0, firstEnd));
    if (first.substr(0, MAGIC.size()) != MAGIC) {
        error = "not an Aevora Engine project";
        return false;
    }

    u32 version = 0;
    const std::string_view number = trim(first.substr(MAGIC.size()));
    if (number.empty()) {
        error = "not an Aevora Engine project";
        return false;
    }
    for (char digit : number) {
        if (digit < '0' || digit > '9' || version > 100000) {
            error = "not an Aevora Engine project";
            return false;
        }
        version = version * 10 + static_cast<u32>(digit - '0');
    }
    if (version > VERSION) {
        error = "made by a newer version of Aevora Engine (format " + std::to_string(version) + ")";
        return false;
    }

    // 2. One key = value per line
    Project result;
    bool hasName = false;
    u32 lineNumber = 1;
    std::string_view rest = firstEnd == std::string_view::npos ? std::string_view() : text.substr(firstEnd + 1);
    while (!rest.empty()) {
        const std::size_t end = rest.find('\n');
        const std::string_view line = trim(rest.substr(0, end));
        rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
        ++lineNumber;
        if (line.empty() || line.front() == '#') continue;

        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            error = "line " + std::to_string(lineNumber) + " has no '='";
            return false;
        }

        const std::string_view key = trim(line.substr(0, equals));
        if (key == "name") {
            if (!unquote(trim(line.substr(equals + 1)), result.name)) {
                error = "line " + std::to_string(lineNumber) + ": the name must be in quotes";
                return false;
            }
            hasName = true;
        }
    }

    if (!hasName || result.name.empty()) {
        error = "the project has no name";
        return false;
    }

    project.name = result.name;
    return true;
}

bool ProjectFile::save(const Project& project, std::string& error) {
    std::filesystem::path temporary = project.file;
    temporary += ".tmp";

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        const std::string text = write(project);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!file) {
            error = "couldn't write " + pathText(project.file);
            return false;
        }
    }

    std::error_code code;
    std::filesystem::rename(temporary, project.file, code);
    if (code) {
        std::filesystem::remove(temporary, code);
        error = "couldn't write " + pathText(project.file);
        return false;
    }
    return true;
}

bool ProjectFile::load(const std::filesystem::path& file, Project& project, std::string& error) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        error = "couldn't open " + pathText(file);
        return false;
    }

    std::ostringstream text;
    text << stream.rdbuf();

    Project result;
    if (!read(text.str(), result, error)) return false;
    result.file = std::filesystem::absolute(file);
    project = result;
    return true;
}

std::filesystem::path ProjectFile::find(const std::filesystem::path& folder) {
    std::error_code code;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, code)) {
        if (entry.is_regular_file(code) && entry.path().extension() == EXTENSION) return entry.path();
    }
    return {};
}

bool ProjectFile::create(const std::filesystem::path& folder, Project& project, std::string& error) {
    const std::filesystem::path absolute = std::filesystem::absolute(folder).lexically_normal();
    // A path ending in a separator has an empty last part; the folder's name is the part before it
    const std::filesystem::path named = absolute.has_filename() ? absolute : absolute.parent_path();
    const std::string name = pathText(named.filename());
    if (name.empty()) {
        error = "the project folder needs a name";
        return false;
    }

    if (!find(named).empty()) {
        error = pathText(named) + " already holds a project; open it instead";
        return false;
    }

    std::error_code code;
    std::filesystem::create_directories(named, code);
    for (const char* sub : FOLDERS) std::filesystem::create_directories(named / sub, code);
    if (code) {
        error = "couldn't make " + pathText(named);
        return false;
    }

    Project result;
    result.name = name;
    result.file = named / (named.filename().native() + std::filesystem::path(EXTENSION).native());
    if (!save(result, error)) return false;
    project = result;
    return true;
}
