#include "application/light_commands.hpp"
#include "application/command_parsing.hpp"

#include <iostream>
#include <optional>

namespace {
    constexpr f32 DEGREES_TO_RADIANS = 3.14159265f / 180.0f;
    constexpr f32 RADIANS_TO_DEGREES = 180.0f / 3.14159265f;

    const char* USAGE =
        "usage: light list\n"
        "       light ambient [color <r> <g> <b> | strength <s>]\n"
        "       light add <point | directional | spot> [name]\n"
        "       light <id> [remove | on | off | name <n> | type <t> | color <r> <g> <b> | intensity <i>\n"
        "                   | position <x> <y> <z> | direction <x> <y> <z> | range <r> | cone <inner> <outer>]";

    std::optional<LightType> parseLightType(const std::string& text) {
        if (text == "point") return LightType::Point;
        if (text == "directional") return LightType::Directional;
        if (text == "spot") return LightType::Spot;
        return std::nullopt;
    }

    const char* lightTypeName(LightType type) {
        switch (type) {
            case LightType::Point: return "point";
            case LightType::Directional: return "directional";
            case LightType::Spot: return "spot";
        }
        return "unknown";
    }

    void printVec3(const Vec3& v) {
        std::cout << v.x << " " << v.y << " " << v.z;
    }

    void printLight(LightHandle handle, const Light& light) {
        std::cout << "[light " << handle.index << "] " << (light.name.empty() ? "(unnamed)" : light.name)
                  << ": " << lightTypeName(light.type) << ", " << (light.enabled ? "on" : "off")
                  << ", color ";
        printVec3(light.color);
        std::cout << ", intensity " << light.intensity;

        if (light.type != LightType::Directional) {
            std::cout << ", position ";
            printVec3(light.position);
            std::cout << ", range " << light.range;
        }

        if (light.type != LightType::Point) {
            std::cout << ", direction ";
            printVec3(light.direction);
        }

        if (light.type == LightType::Spot) {
            std::cout << ", cone " << light.innerConeRadians * RADIANS_TO_DEGREES
                      << " " << light.outerConeRadians * RADIANS_TO_DEGREES;
        }

        std::cout << std::endl;
    }

    void printAmbient(const AmbientLight& ambient) {
        std::cout << "[light ambient] color ";
        printVec3(ambient.color);
        std::cout << ", strength " << ambient.strength << std::endl;
    }

    void runAmbient(AppContext& ctx, const CommandArgs& args) {
        LightCollection& lights = ctx.scene.lights;

        if (args.size() == 1) {
            printAmbient(lights.getAmbient());
            return;
        }

        if (args[1] == "color") {
            Vec3 color;
            if (!parseVec3Args(args, 2, color, true)) {
                std::cout << "usage: light ambient color <r> <g> <b>  (each 0 to 1)" << std::endl;
                return;
            }

            ctx.history.begin(ctx.scene);
            lights.setAmbientColor(color);
            ctx.history.commit();
        } else if (args[1] == "strength") {
            f32 strength = 0.0f;
            if (args.size() != 3 || !parseUnitFloat(args[2], strength)) {
                std::cout << "usage: light ambient strength <0 to 1>" << std::endl;
                return;
            }

            ctx.history.begin(ctx.scene);
            lights.setAmbientStrength(strength);
            ctx.history.commit();
        } else {
            std::cout << "light ambient: unknown property '" << args[1] << "' (color, strength)" << std::endl;
            return;
        }

        printAmbient(lights.getAmbient());
    }

    void runList(const LightCollection& lights) {
        if (lights.count() == 0) std::cout << "[light] no lights" << std::endl;

        for (LightHandle handle : lights.handles()) {
            printLight(handle, lights.get(handle));
        }

        printAmbient(lights.getAmbient());
    }

    void runAdd(AppContext& ctx, const CommandArgs& args) {
        const std::optional<LightType> type = args.size() >= 2 ? parseLightType(args[1]) : std::nullopt;
        if (!type || args.size() > 3) {
            std::cout << "usage: light add <point | directional | spot> [name]" << std::endl;
            return;
        }

        Light light;
        light.type = *type;
        light.name = args.size() == 3 ? args[2] : ctx.scene.lights.nextName();

        ctx.history.begin(ctx.scene);
        const LightHandle handle = ctx.scene.lights.add(light);
        ctx.history.commit();

        printLight(handle, ctx.scene.lights.get(handle));
    }

    // Applies "light <id> <property> ..." to the light; prints usage and returns false on bad input
    bool editLight(Light& light, const CommandArgs& args) {
        const std::string& property = args[1];

        if (property == "on" && args.size() == 2) {
            light.enabled = true;
        } else if (property == "off" && args.size() == 2) {
            light.enabled = false;
        } else if (property == "name" && args.size() == 3) {
            light.name = args[2];
        } else if (property == "type" && args.size() == 3 && parseLightType(args[2])) {
            light.type = *parseLightType(args[2]);
        } else if (property == "color") {
            if (!parseVec3Args(args, 2, light.color, true)) {
                std::cout << "usage: light <id> color <r> <g> <b>  (each 0 to 1)" << std::endl;
                return false;
            }
        } else if (property == "intensity") {
            f32 intensity = 0.0f;
            if (args.size() != 3 || !parseFloat(args[2], intensity) || intensity < 0.0f) {
                std::cout << "usage: light <id> intensity <i>  (0 or more)" << std::endl;
                return false;
            }
            light.intensity = intensity;
        } else if (property == "position") {
            if (!parseVec3Args(args, 2, light.position, false)) {
                std::cout << "usage: light <id> position <x> <y> <z>" << std::endl;
                return false;
            }
        } else if (property == "direction") {
            Vec3 direction;
            if (!parseVec3Args(args, 2, direction, false) || direction.length() < 1e-6f) {
                std::cout << "usage: light <id> direction <x> <y> <z>  (not all zero)" << std::endl;
                return false;
            }
            light.direction = direction.normalized();
        } else if (property == "range") {
            f32 range = 0.0f;
            if (args.size() != 3 || !parseFloat(args[2], range) || range <= 0.0f) {
                std::cout << "usage: light <id> range <r>  (more than 0)" << std::endl;
                return false;
            }
            light.range = range;
        } else if (property == "cone") {
            f32 inner = 0.0f;
            f32 outer = 0.0f;
            if (args.size() != 4 || !parseFloat(args[2], inner) || !parseFloat(args[3], outer)
                || inner < 0.0f || outer <= 0.0f || inner > outer || outer >= 90.0f) {
                std::cout << "usage: light <id> cone <inner> <outer>  (half-angles in degrees, 0 <= inner <= outer < 90)" << std::endl;
                return false;
            }
            light.innerConeRadians = inner * DEGREES_TO_RADIANS;
            light.outerConeRadians = outer * DEGREES_TO_RADIANS;
        } else {
            std::cout << USAGE << std::endl;
            return false;
        }

        return true;
    }

    void runLightById(AppContext& ctx, const CommandArgs& args) {
        LightCollection& lights = ctx.scene.lights;

        u32 slot = 0;
        const LightHandle handle = parseU32(args[0], slot) ? lights.handleAt(slot) : INVALID_LIGHT;
        if (!lights.isValid(handle)) {
            std::cout << "light: no light with id '" << args[0] << "' (see light list)" << std::endl;
            return;
        }

        if (args.size() == 1) {
            printLight(handle, lights.get(handle));
            return;
        }

        if (args[1] == "remove" && args.size() == 2) {
            ctx.history.begin(ctx.scene);
            lights.remove(handle);
            ctx.history.commit();

            std::cout << "[light " << handle.index << "] removed" << std::endl;
            return;
        }

        Light edited = lights.get(handle);
        if (!editLight(edited, args)) return;

        ctx.history.begin(ctx.scene);
        lights.replace(handle, edited);
        ctx.history.commit();

        printLight(handle, lights.get(handle));
    }
}

void runLightCommand(AppContext& ctx, const CommandArgs& args) {
    if (args.empty()) {
        std::cout << USAGE << std::endl;
    } else if (args[0] == "list") {
        runList(ctx.scene.lights);
    } else if (args[0] == "ambient") {
        runAmbient(ctx, args);
    } else if (args[0] == "add") {
        runAdd(ctx, args);
    } else {
        runLightById(ctx, args);
    }
}

bool deleteSelectedLights(AppContext& ctx) {
    const std::vector<LightHandle> selected = ctx.scene.selection.getLights();
    if (selected.empty()) return false;

    ctx.history.begin(ctx.scene);
    for (LightHandle handle : selected) ctx.scene.lights.remove(handle);
    ctx.scene.selection.clearLights();
    ctx.history.commit();
    return true;
}
