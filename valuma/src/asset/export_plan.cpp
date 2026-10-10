#include "asset/export_plan.hpp"
#include "asset/asset_file.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_set>

namespace {
    constexpr std::string_view FORBIDDEN = "\\/:*?\"<>|";

    std::string lowercase(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string sanitize(const std::string& name) {
        std::string result;
        for (char character : name) {
            const unsigned char c = static_cast<unsigned char>(character);
            result += (c < 32 || FORBIDDEN.find(character) != std::string_view::npos) ? '_' : character;
        }

        // Windows drops trailing dots and spaces from file names, so they'd silently change the name
        while (!result.empty() && (result.back() == '.' || result.back() == ' ')) result.pop_back();
        return result.empty() ? "Untitled" : result;
    }
}

std::string assetFileName(const std::string& objectName) {
    return sanitize(objectName) + AssetFile::EXTENSION;
}

void planExport(std::vector<ExportPlanItem>& items, const std::function<bool(const std::string& fileName)>& fileExists) {
    // File names (lowercase) already given to earlier checked items
    std::unordered_set<std::string> claimed;

    auto free = [&](const std::string& fileName) {
        return !claimed.contains(lowercase(fileName)) && !fileExists(fileName);
    };

    for (ExportPlanItem& item : items) {
        const std::string base = sanitize(item.objectName);
        const std::string fileName = base + AssetFile::EXTENSION;

        item.renamed = false;
        item.fileName = fileName;

        if (item.checked && claimed.contains(lowercase(fileName))) item.status = ExportStatus::Duplicate;
        else if (fileExists(fileName)) item.status = ExportStatus::Exists;
        else item.status = ExportStatus::New;

        const bool rename = item.status == ExportStatus::Duplicate || (item.status == ExportStatus::Exists && item.choice == CollisionChoice::Rename);
        if (rename) {
            for (u32 number = 2;; ++number) {
                const std::string candidate = base + " " + std::to_string(number) + AssetFile::EXTENSION;
                if (free(candidate)) {
                    item.fileName = candidate;
                    item.renamed = true;
                    break;
                }
            }
        }

        const bool skipped = item.status == ExportStatus::Exists && item.choice == CollisionChoice::Skip;
        item.write = item.checked && !skipped;
        if (item.write) claimed.insert(lowercase(item.fileName));
    }
}
