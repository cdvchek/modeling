#include "application/commands/texture_commands.hpp"
#include "application/commands/command_parsing.hpp"
#include "application/actions/project_actions.hpp"
#include "application/viewport/pictures.hpp"
#include "platform/platform.hpp"

#include <iostream>

namespace {
    constexpr const char* IMAGE_TYPE = "PNG image";

    const char* USAGE =
        "usage: texture list\n"
        "       texture load <path>  (a PNG file)\n"
        "       texture <id> [remove | reload | name <n>]\n"
        "       (material <id> map <texture id | none> gives a material its base color map)";

    void printTexture(const AppContext& ctx, TextureHandle handle) {
        const Texture& texture = ctx.scene.textures.get(handle);
        std::cout << "[texture " << handle.index << "] " << texture.name;
        if (texture.picture) std::cout << ": " << texture.picture->width << " x " << texture.picture->height << ", " << texture.picture->fileName;
        std::cout << ", used by " << describeTextureUse(textureUse(ctx, handle)) << std::endl;
    }
}

TextureHandle loadTexture(AppContext& ctx, const std::filesystem::path& path, MaterialHandle material) {
    const std::string fileName = utf8(path.filename());

    std::string error;
    std::shared_ptr<const Picture> picture = loadPicture(path, error);
    if (!picture) {
        ctx.systems.console.printError("Couldn't load " + fileName + ": " + error);
        return INVALID_TEXTURE;
    }

    Texture texture;
    texture.name = ctx.scene.textures.uniqueName(utf8(path.stem()));
    texture.picture = std::move(picture);
    texture.sourcePath = utf8(std::filesystem::absolute(path));
    const std::string size = std::to_string(texture.picture->width) + " x " + std::to_string(texture.picture->height);

    // Adding it and putting it on the material are one step
    ctx.history.begin(ctx.scene);
    const TextureHandle handle = ctx.scene.textures.add(std::move(texture));
    if (Material* target = ctx.scene.materials.tryGet(material)) target->baseColorMap = handle;
    ctx.history.commit();

    ctx.textureFolder = path.parent_path();
    ctx.viewport.selectedTexture = handle;
    ctx.systems.console.print("Loaded " + ctx.scene.textures.get(handle).name + " (" + fileName + ", " + size + ")");
    return handle;
}

void chooseTexture(AppContext& ctx, MaterialHandle material) {
    std::error_code code;
    const std::filesystem::path folder = std::filesystem::is_directory(ctx.textureFolder, code) ? ctx.textureFolder : projectsFolder();

    const std::filesystem::path path = Platform::chooseOpenFile(nativeWindow(ctx), IMAGE_TYPE, "png", folder);
    afterDialog(ctx);
    if (!path.empty()) loadTexture(ctx, path, material);
}

void setBaseColorMap(AppContext& ctx, MaterialHandle material, TextureHandle texture) {
    Material* target = ctx.scene.materials.tryGet(material);
    if (!target) return;

    ctx.history.begin(ctx.scene);
    target->baseColorMap = texture;
    ctx.history.commit();
}

void removeTexture(AppContext& ctx, TextureHandle texture) {
    if (!ctx.scene.textures.isValid(texture)) return;

    // Materials keep the stale handle, which reads as no map; undo brings the texture back in its slot
    ctx.history.begin(ctx.scene);
    ctx.scene.textures.remove(texture);
    ctx.history.commit();
}

bool reloadTexture(AppContext& ctx, TextureHandle texture) {
    Texture* target = ctx.scene.textures.tryGet(texture);
    if (!target) return false;
    if (target->sourcePath.empty()) {
        ctx.systems.console.printError("Couldn't reload " + target->name + ": it has no file to read (it came from an asset)");
        return false;
    }

    const std::u8string text(target->sourcePath.begin(), target->sourcePath.end());
    std::string error;
    std::shared_ptr<const Picture> picture = loadPicture(std::filesystem::path(text), error);
    if (!picture) {
        ctx.systems.console.printError("Couldn't reload " + target->name + ": " + error);
        return false;
    }

    ctx.history.begin(ctx.scene);
    ctx.scene.textures.get(texture).picture = std::move(picture);
    ctx.history.commit();

    const Texture& reloaded = ctx.scene.textures.get(texture);
    ctx.systems.console.print("Reloaded " + reloaded.name + " (" + std::to_string(reloaded.picture->width) + " x " + std::to_string(reloaded.picture->height) + ")");
    return true;
}

u32 textureUse(const AppContext& ctx, TextureHandle texture) {
    if (!ctx.scene.textures.isValid(texture)) return 0;
    u32 count = 0;
    for (MaterialHandle handle : ctx.scene.materials.handles()) {
        if (ctx.scene.materials.get(handle).baseColorMap == texture) ++count;
    }
    return count;
}

std::string describeTextureUse(u32 materials) {
    if (materials == 0) return "unused";
    return std::to_string(materials) + (materials == 1 ? " material" : " materials");
}

void runTextureCommand(AppContext& ctx, const CommandArgs& args) {
    TextureCollection& textures = ctx.scene.textures;

    if (args.empty() || (args[0] == "list" && args.size() == 1)) {
        if (textures.count() == 0) std::cout << "[texture] no textures" << std::endl;
        for (TextureHandle handle : textures.handles()) printTexture(ctx, handle);
        return;
    }

    if (args[0] == "load") {
        if (args.size() < 2) {
            std::cout << "usage: texture load <path>" << std::endl;
            return;
        }
        // A path with spaces arrives split; join it back
        std::string path = args[1];
        for (std::size_t i = 2; i < args.size(); ++i) path += " " + args[i];
        const std::u8string text(path.begin(), path.end());
        const TextureHandle handle = loadTexture(ctx, std::filesystem::path(text));
        if (textures.isValid(handle)) printTexture(ctx, handle);
        return;
    }

    u32 slot = 0;
    const TextureHandle handle = parseU32(args[0], slot) ? textures.handleAt(slot) : INVALID_TEXTURE;
    if (!textures.isValid(handle)) {
        std::cout << "texture: no texture with id '" << args[0] << "' (see texture list)" << std::endl;
        return;
    }

    if (args.size() == 1) {
        printTexture(ctx, handle);
    } else if (args[1] == "remove" && args.size() == 2) {
        const std::string name = textures.get(handle).name;
        removeTexture(ctx, handle);
        std::cout << "[texture " << handle.index << "] " << name << " removed" << std::endl;
    } else if (args[1] == "reload" && args.size() == 2) {
        if (reloadTexture(ctx, handle)) printTexture(ctx, handle);
    } else if (args[1] == "name" && args.size() == 3) {
        ctx.history.begin(ctx.scene);
        textures.get(handle).name = args[2];
        ctx.history.commit();
        printTexture(ctx, handle);
    } else {
        std::cout << USAGE << std::endl;
    }
}
