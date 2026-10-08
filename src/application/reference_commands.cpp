#include "application/reference_commands.hpp"
#include "application/command_parsing.hpp"
#include "application/project_actions.hpp"
#include "application/reference_images.hpp"

#include <iostream>
#include <optional>

namespace {
    constexpr f32 DEGREES_TO_RADIANS = 3.14159265f / 180.0f;
    constexpr f32 RADIANS_TO_DEGREES = 180.0f / 3.14159265f;

    const char* USAGE =
        "usage: reference list\n"
        "       reference add [<file.png>]\n"
        "       reference <id> [remove | show | hide | lock | unlock | name <n> | opacity <0 to 1>\n"
        "                       | depth <behind | scene | front> | position <x> <y> <z> | rotation <x> <y> <z> | size <s>]";

    std::optional<ReferenceDepth> parseDepth(const std::string& text) {
        if (text == "behind") return ReferenceDepth::Behind;
        if (text == "scene") return ReferenceDepth::InScene;
        if (text == "front") return ReferenceDepth::InFront;
        return std::nullopt;
    }

    void printVec3(const Vec3& v) {
        std::cout << v.x << " " << v.y << " " << v.z;
    }

    void printReference(ReferenceHandle handle, const ReferenceImage& image) {
        std::cout << "[reference " << handle.index << "] " << image.name;
        if (image.picture) std::cout << " (" << image.picture->fileName << ", " << image.picture->width << " x " << image.picture->height << ")";
        std::cout << ": " << (image.visible ? "shown" : "hidden") << (image.locked ? ", locked" : "")
                  << ", opacity " << image.opacity << ", depth " << referenceDepthName(image.depth)
                  << ", position ";
        printVec3(image.position);
        std::cout << ", rotation ";
        printVec3(image.rotation * RADIANS_TO_DEGREES);
        std::cout << ", size " << image.size << std::endl;
    }

    void runAdd(AppContext& ctx, const CommandArgs& args) {
        if (args.size() == 1) {
            chooseReferenceImages(ctx);
            return;
        }

        // The rest of the line is the path (spaces allowed, quotes optional); relative paths start in the projects folder
        std::string text;
        for (std::size_t i = 1; i < args.size(); ++i) text += (text.empty() ? "" : " ") + args[i];
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"') text = text.substr(1, text.size() - 2);

        std::filesystem::path path(std::u8string(text.begin(), text.end()));
        if (!path.has_extension()) path += ".png";
        if (path.is_relative()) path = projectsFolder() / path;
        addReferenceFrom(ctx, path);
    }

    // Applies "reference <id> <property> ..." to the image; prints usage and returns false on bad input
    bool editReference(ReferenceImage& image, const CommandArgs& args) {
        const std::string& property = args[1];

        if (property == "show" && args.size() == 2) {
            image.visible = true;
        } else if (property == "hide" && args.size() == 2) {
            image.visible = false;
        } else if (property == "lock" && args.size() == 2) {
            image.locked = true;
        } else if (property == "unlock" && args.size() == 2) {
            image.locked = false;
        } else if (property == "name" && args.size() == 3) {
            image.name = args[2];
        } else if (property == "opacity") {
            if (args.size() != 3 || !parseUnitFloat(args[2], image.opacity)) {
                std::cout << "usage: reference <id> opacity <0 to 1>" << std::endl;
                return false;
            }
        } else if (property == "depth") {
            const std::optional<ReferenceDepth> depth = args.size() == 3 ? parseDepth(args[2]) : std::nullopt;
            if (!depth) {
                std::cout << "usage: reference <id> depth <behind | scene | front>" << std::endl;
                return false;
            }
            image.depth = *depth;
        } else if (property == "position") {
            if (!parseVec3Args(args, 2, image.position, false)) {
                std::cout << "usage: reference <id> position <x> <y> <z>" << std::endl;
                return false;
            }
        } else if (property == "rotation") {
            Vec3 degrees;
            if (!parseVec3Args(args, 2, degrees, false)) {
                std::cout << "usage: reference <id> rotation <x> <y> <z>  (degrees)" << std::endl;
                return false;
            }
            image.rotation = degrees * DEGREES_TO_RADIANS;
        } else if (property == "size") {
            f32 size = 0.0f;
            if (args.size() != 3 || !parseFloat(args[2], size) || size < MIN_REFERENCE_SIZE) {
                std::cout << "usage: reference <id> size <s>  (height in units, more than 0)" << std::endl;
                return false;
            }
            image.size = size;
        } else {
            std::cout << USAGE << std::endl;
            return false;
        }

        return true;
    }

    void runReferenceById(AppContext& ctx, const CommandArgs& args) {
        ReferenceCollection& references = ctx.scene.references;

        u32 slot = 0;
        const ReferenceHandle handle = parseU32(args[0], slot) ? references.handleAt(slot) : INVALID_REFERENCE;
        if (!references.isValid(handle)) {
            std::cout << "reference: no image with id '" << args[0] << "' (see reference list)" << std::endl;
            return;
        }

        if (args.size() == 1) {
            printReference(handle, references.get(handle));
            return;
        }

        if (args[1] == "remove" && args.size() == 2) {
            ctx.history.begin(ctx.scene);
            references.remove(handle);
            ctx.scene.selection.removeReference(handle);
            ctx.history.commit();

            std::cout << "[reference " << handle.index << "] removed" << std::endl;
            return;
        }

        ReferenceImage edited = references.get(handle);
        if (!editReference(edited, args)) return;

        ctx.history.begin(ctx.scene);
        references.get(handle) = edited;
        ctx.history.commit();

        printReference(handle, references.get(handle));
    }
}

void runReferenceCommand(AppContext& ctx, const CommandArgs& args) {
    if (args.empty()) {
        std::cout << USAGE << std::endl;
    } else if (args[0] == "list") {
        if (ctx.scene.references.count() == 0) std::cout << "[reference] no reference images" << std::endl;
        for (ReferenceHandle handle : ctx.scene.references.handles()) printReference(handle, ctx.scene.references.get(handle));
    } else if (args[0] == "add") {
        if (canUseProjectFiles(ctx)) runAdd(ctx, args);
    } else {
        runReferenceById(ctx, args);
    }
}
