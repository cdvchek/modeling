#include "application/commands/layer_commands.hpp"
#include "application/commands/command_parsing.hpp"
#include "application/paint/paint_workspace.hpp"
#include "scene/textures/paint_targets.hpp"

#include <algorithm>
#include <iostream>

namespace {
    const char* USAGE =
        "usage: layer list\n"
        "       layer add [name]\n"
        "       layer <n> [active | remove | show | hide | opacity <0-1> | name <n> | move <place> | fill <r> <g> <b> [a] | clear]\n"
        "       (the layers of the texture being painted; 0 is the bottom one; colors are 0 to 1)";

    void printLayers(const Texture& texture) {
        const LayerStack& stack = texture.layers;
        if (stack.empty()) {
            std::cout << "[layer] " << texture.name << " has no layers yet (its picture becomes Base when one is added or painted)" << std::endl;
            return;
        }

        // Top first, as they stack
        std::cout << "[layer] " << texture.name << ", " << stack.width << " x " << stack.height << std::endl;
        for (std::size_t i = stack.layers.size(); i-- > 0;) {
            const Layer& layer = stack.layers[i];
            const std::size_t painted = std::count_if(layer.tiles.begin(), layer.tiles.end(), [](const std::shared_ptr<Tile>& tile) { return tile != nullptr; });
            std::cout << "  " << (i == stack.active ? "> " : "  ") << i << " " << layer.name << ": opacity " << layer.opacity
                      << (layer.visible ? "" : ", hidden") << ", " << painted << " of " << layer.tiles.size() << " tiles painted" << std::endl;
        }
    }

    u8 channel(f32 value) {
        return static_cast<u8>(std::lround(value * 255.0f));
    }
}

LayerStack* paintLayers(AppContext& ctx) {
    Texture* texture = ctx.scene.textures.tryGet(activePaintTexture(ctx));
    if (!texture) {
        ctx.systems.console.printError("layer: no texture is being painted (pick one in the Paint workspace's Texture list)");
        return nullptr;
    }
    std::string error;
    if (!makeLayered(*texture, error)) {
        ctx.systems.console.printError("layer: couldn't read " + texture->name + ": " + error);
        return nullptr;
    }
    return &texture->layers;
}

void runLayerCommand(AppContext& ctx, const CommandArgs& args) {
    if (args.empty() || (args[0] == "list" && args.size() == 1)) {
        const Texture* texture = ctx.scene.textures.tryGet(activePaintTexture(ctx));
        if (texture) printLayers(*texture);
        else std::cout << "[layer] no texture is being painted" << std::endl;
        return;
    }

    // Everything else changes the layers, as one undo step; a refused change leaves no step behind
    ctx.history.begin(ctx.scene);
    LayerStack* stack = paintLayers(ctx);
    if (!stack) {
        ctx.history.cancel(ctx.scene);
        return;
    }

    bool done = false;
    u32 index = 0;
    if (args[0] == "add") {
        std::string name;
        for (std::size_t i = 1; i < args.size(); ++i) name += (i > 1 ? " " : "") + args[i];
        addLayer(*stack, name);
        done = true;
    } else if (!parseU32(args[0], index) || index >= stack->layers.size()) {
        std::cout << "layer: no layer '" << args[0] << "' (see layer list)" << std::endl;
    } else if (args.size() == 1) {
        std::cout << USAGE << std::endl;
    } else if (args[1] == "active" && args.size() == 2) {
        stack->active = index;
        done = true;
    } else if (args[1] == "remove" && args.size() == 2) {
        done = removeLayer(*stack, index);
        if (!done) std::cout << "layer: the only layer can't be removed" << std::endl;
    } else if ((args[1] == "show" || args[1] == "hide") && args.size() == 2) {
        stack->layers[index].visible = args[1] == "show";
        done = true;
    } else if (args[1] == "opacity" && args.size() == 3) {
        done = parseUnitFloat(args[2], stack->layers[index].opacity);
        if (!done) std::cout << "layer: opacity is 0 to 1" << std::endl;
    } else if (args[1] == "name" && args.size() >= 3) {
        std::string name = args[2];
        for (std::size_t i = 3; i < args.size(); ++i) name += " " + args[i];
        stack->layers[index].name = name;
        done = true;
    } else if (args[1] == "move" && args.size() == 3) {
        u32 place = 0;
        done = parseU32(args[2], place) && moveLayer(*stack, index, place);
        if (!done) std::cout << "layer: no place '" << args[2] << "' (0 to " << stack->layers.size() - 1 << ")" << std::endl;
    } else if (args[1] == "fill" && (args.size() == 5 || args.size() == 6)) {
        f32 red = 0.0f, green = 0.0f, blue = 0.0f, alpha = 1.0f;
        done = parseUnitFloat(args[2], red) && parseUnitFloat(args[3], green) && parseUnitFloat(args[4], blue)
            && (args.size() == 5 || parseUnitFloat(args[5], alpha));
        if (done) fillLayer(*stack, index, { 0, 0, stack->width, stack->height }, { channel(red), channel(green), channel(blue), channel(alpha) });
        else std::cout << "layer: colors are 0 to 1" << std::endl;
    } else if (args[1] == "clear" && args.size() == 2) {
        fillLayer(*stack, index, { 0, 0, stack->width, stack->height }, {});
        done = true;
    } else {
        std::cout << USAGE << std::endl;
    }

    if (!done) {
        ctx.history.cancel(ctx.scene);
        return;
    }
    ctx.history.commit();
    printLayers(ctx.scene.textures.get(activePaintTexture(ctx)));
}
