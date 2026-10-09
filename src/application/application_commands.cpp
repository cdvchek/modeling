#include "application/application.hpp"
#include "application/actions/editing_actions.hpp"
#include "application/commands/command_parsing.hpp"
#include "application/commands/light_commands.hpp"
#include "application/commands/reference_commands.hpp"
#include "application/commands/material_commands.hpp"
#include "application/commands/shading_commands.hpp"
#include "application/commands/object_commands.hpp"
#include "application/actions/project_actions.hpp"
#include "application/actions/asset_actions.hpp"
#include "application/actions/origin_actions.hpp"
#include "asset/asset_file.hpp"
#include "project/project_file.hpp"

#include <algorithm>
#include <iostream>

namespace {
    void printHeadlight(const Headlight& headlight) {
        std::cout << "[headlight] " << (headlight.enabled ? "on" : "off")
                  << ", color " << headlight.color.x << " " << headlight.color.y << " " << headlight.color.z
                  << ", strength " << headlight.strength << std::endl;
    }

    void printBackFaceTint(const Vec3& tint) {
        std::cout << "[backface tint] " << tint.x << " " << tint.y << " " << tint.z << std::endl;
    }

    // The arguments as one path, so names with spaces work; surrounding quotes are dropped
    std::filesystem::path pathFromArgs(const CommandArgs& args) {
        std::string text;
        for (const std::string& arg : args) text += (text.empty() ? "" : " ") + arg;
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"') text = text.substr(1, text.size() - 2);
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
}

void Application::registerCommands(AppContext& ctx) {
    ctx.systems.console.setEcho(true);

    ctx.systems.commands.registerCommand(
        "help",
        "Lists every command and what it does: help",
        [&ctx](const CommandArgs& args) {
            const auto commands = ctx.systems.commands.list();

            std::size_t width = 0;
            for (const auto& [name, description] : commands) width = std::max(width, name.size());

            for (const auto& [name, description] : commands) {
                std::cout << name << std::string(width - name.size() + 2, ' ') << description << '\n';
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "debug",
        "Shows or hides the half-edge debug overlay: debug [on | off]",
        [&ctx](const CommandArgs& args) {
            if (!args.empty()) {
                if (args[0] == "on") {
                    ctx.systems.input_ctx.addContext(InputContext_Debug);
                }
                else if (args[0] == "off") {
                    ctx.systems.input_ctx.removeContext(InputContext_Debug);
                }
            } else {
                ctx.systems.input_ctx.toggleContext(InputContext_Debug);
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "validate",
        "Checks the active object's mesh and reports the first problem: validate",
        [&ctx](const CommandArgs& args) {
            const Object* object = ctx.scene.activeObject();
            if (!object) {
                std::cout << "[mesh validate] no active object" << std::endl;
                return;
            }

            const MeshData& mesh = object->meshData;

            if (mesh.validate()) {
                std::cout << "[mesh validate] ok ("
                          << mesh.getVertexHandles().size() << " vertices, "
                          << mesh.getEdgeHandles().size() / 2 << " edges, "
                          << mesh.getFaceHandles().size() << " faces)" << std::endl;
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "merge",
        "Merges two selected vertices into one: merge [center | first | last]",
        [&ctx](const CommandArgs& args) {
            if (!canMergeVertices(ctx)) return;

            u8 mergeType = 0;
            if (!args.empty() && args[0] == "first") mergeType = 1;
            else if (!args.empty() && args[0] == "last") mergeType = 2;

            mergeVertices(ctx, mergeType);
        }
    );

    ctx.systems.commands.registerCommand(
        "dissolve",
        "Collapses the selected edge or face into one vertex: dissolve",
        [&ctx](const CommandArgs& args) {
            if (canDissolve(ctx)) dissolveSelection(ctx);
        }
    );

    ctx.systems.commands.registerCommand(
        "light",
        "Edits lights: light list | add <type> | <id> <property> <value> | ambient ...",
        [&ctx](const CommandArgs& args) {
            runLightCommand(ctx, args);
        }
    );

    ctx.systems.commands.registerCommand(
        "material",
        "Adds, edits, and assigns materials: material list | add [name] | <id> <property> <value> | <id> assign [<object id> ...]",
        [&ctx](const CommandArgs& args) {
            runMaterialCommand(ctx, args);
        }
    );

    ctx.systems.commands.registerCommand(
        "shading",
        "Flat, smooth, or auto shading, and hard or smooth edges: shading flat | smooth | auto [<degrees>] | mark hard | smooth | clear",
        [&ctx](const CommandArgs& args) {
            runShadingCommand(ctx, args);
        }
    );

    ctx.systems.commands.registerCommand(
        "reference",
        "Adds and edits reference images: reference list | add [<file.png>] | <id> <property> <value>",
        [&ctx](const CommandArgs& args) {
            runReferenceCommand(ctx, args);
        }
    );

    ctx.systems.commands.registerCommand(
        "backface",
        "Shows or sets the tint on back faces: backface tint [<r> <g> <b>]",
        [&ctx](const CommandArgs& args) {
            if (args.empty() || args[0] != "tint") {
                std::cout << "usage: backface tint [<r> <g> <b>]  (each 0 to 1, 1 1 1 = no tint)" << std::endl;
                return;
            }

            if (args.size() == 1) {
                printBackFaceTint(ctx.renderer->getBackFaceTint());
                return;
            }

            Vec3 tint;
            if (args.size() != 4
                || !parseUnitFloat(args[1], tint.x)
                || !parseUnitFloat(args[2], tint.y)
                || !parseUnitFloat(args[3], tint.z)) {
                std::cout << "usage: backface tint <r> <g> <b>  (each 0 to 1, 1 1 1 = no tint)" << std::endl;
                return;
            }

            ctx.renderer->setBackFaceTint(tint);
            printBackFaceTint(tint);
        }
    );

    ctx.systems.commands.registerCommand(
        "stats",
        "Shows or hides frame times, draw calls, primitives, uploads, and the scene's size: stats [on | off]",
        [&ctx](const CommandArgs& args) {
            if (args.empty()) ctx.viewport.showStats = !ctx.viewport.showStats;
            else if (args.size() == 1 && (args[0] == "on" || args[0] == "off")) ctx.viewport.showStats = args[0] == "on";
            else {
                std::cout << "usage: stats [on | off]" << std::endl;
                return;
            }
            std::cout << "[stats] " << (ctx.viewport.showStats ? "shown" : "hidden") << std::endl;
        }
    );

    ctx.systems.commands.registerCommand(
        "exposure",
        "Shows or sets how bright lit surfaces look, in stops (0 is normal, +1 twice as bright): exposure [<stops>]",
        [&ctx](const CommandArgs& args) {
            f32 stops = 0.0f;
            if (args.size() == 1) {
                if (!parseFloat(args[0], stops) || stops < ViewportSettings::MIN_EXPOSURE || stops > ViewportSettings::MAX_EXPOSURE) {
                    std::cout << "usage: exposure [<stops>]  (-5 to 5)" << std::endl;
                    return;
                }
                ctx.viewport.exposure = stops;
            } else if (!args.empty()) {
                std::cout << "usage: exposure [<stops>]  (-5 to 5)" << std::endl;
                return;
            }
            std::cout << "[exposure] " << ctx.viewport.exposure << " stops" << std::endl;
        }
    );

    ctx.systems.commands.registerCommand(
        "headlight",
        "Changes the camera light: headlight [on | off | color <r> <g> <b> | strength <s>]",
        [&ctx](const CommandArgs& args) {
            Headlight& headlight = ctx.viewport.headlight;
            const char* usage = "usage: headlight [on | off | color <r> <g> <b> | strength <s>]  (values 0 to 1)";

            if (args.empty()) {
                printHeadlight(headlight);
                return;
            }

            if (args[0] == "on" && args.size() == 1) {
                headlight.enabled = true;
            } else if (args[0] == "off" && args.size() == 1) {
                headlight.enabled = false;
            } else if (args[0] == "color") {
                Vec3 color;
                if (args.size() != 4
                    || !parseUnitFloat(args[1], color.x)
                    || !parseUnitFloat(args[2], color.y)
                    || !parseUnitFloat(args[3], color.z)) {
                    std::cout << usage << std::endl;
                    return;
                }
                headlight.color = color;
            } else if (args[0] == "strength") {
                f32 strength = 0.0f;
                if (args.size() != 2 || !parseUnitFloat(args[1], strength)) {
                    std::cout << usage << std::endl;
                    return;
                }
                headlight.strength = strength;
            } else {
                std::cout << usage << std::endl;
                return;
            }

            printHeadlight(headlight);
        }
    );

    ctx.systems.commands.registerCommand(
        "vsync",
        "Turns vertical sync on or off: vsync [on | off]",
        [&ctx](const CommandArgs& args) {
            if (args.empty()) {
                ctx.renderer->setVSync(!ctx.renderer->getVSync());
            } else if (args[0] == "on" && args.size() == 1) {
                ctx.renderer->setVSync(true);
            } else if (args[0] == "off" && args.size() == 1) {
                ctx.renderer->setVSync(false);
            } else {
                std::cout << "usage: vsync [on | off]  (no argument toggles)" << std::endl;
                return;
            }

            std::cout << "[vsync] " << (ctx.renderer->getVSync() ? "on" : "off") << std::endl;
        }
    );

    ctx.systems.commands.registerCommand(
        "ui",
        "Shows or hides the floating panel, switches between material and clay view, or shows the UV grid: ui panel | materials | checker",
        [&ctx](const CommandArgs& args) {
            if (args.size() == 1 && args[0] == "panel") {
                ctx.viewport.showPanel = !ctx.viewport.showPanel;
                std::cout << "[ui panel] " << (ctx.viewport.showPanel ? "shown" : "hidden") << std::endl;
            } else if (args.size() == 1 && args[0] == "materials") {
                toggleMaterials(ctx);
                std::cout << "[ui materials] " << (ctx.viewport.showMaterials ? "material view" : "clay view") << std::endl;
            } else if (args.size() == 1 && args[0] == "checker") {
                toggleUVChecker(ctx);
                std::cout << "[ui checker] " << (ctx.viewport.showUVChecker ? "shown" : "hidden") << std::endl;
            } else {
                std::cout << "usage: ui panel | materials | checker" << std::endl;
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "save",
        "Saves the project, to a new file if given: save [<path>]",
        [&ctx](const CommandArgs& args) {
            if (args.empty()) saveProject(ctx);
            else saveProjectTo(ctx, resolveProjectPath(pathFromArgs(args)));
        }
    );

    ctx.systems.commands.registerCommand(
        "open",
        "Opens a project, asking which one if no path is given: open [<path>]",
        [&ctx](const CommandArgs& args) {
            if (args.empty()) {
                openProject(ctx);
            } else if (canUseProjectFiles(ctx)) {
                const std::filesystem::path path = resolveProjectPath(pathFromArgs(args));
                confirmDiscardChanges(ctx, [&ctx, path] { openProjectFrom(ctx, path); });
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "new",
        "Starts a new project with the default scene: new",
        [&ctx](const CommandArgs& args) {
            newProject(ctx);
        }
    );

    ctx.systems.commands.registerCommand(
        "origin",
        "Moves an origin, leaving the mesh in place: origin geometry | bottom | world | rotation | selection",
        [&ctx](const CommandArgs& args) {
            const char* usage = "usage: origin geometry | bottom | world | rotation | selection  (the selected origin's object, else the active object; selection uses the selected vertices)";
            if (args.size() != 1) {
                std::cout << usage << std::endl;
                return;
            }

            OriginTarget target;
            if (args[0] == "geometry") target = OriginTarget::Geometry;
            else if (args[0] == "bottom") target = OriginTarget::Bottom;
            else if (args[0] == "world") target = OriginTarget::World;
            else if (args[0] == "rotation") target = OriginTarget::WorldRotation;
            else if (args[0] == "selection") target = OriginTarget::Selection;
            else {
                std::cout << usage << std::endl;
                return;
            }

            if (!canUseProjectFiles(ctx)) return;
            if (moveOrigin(ctx, target)) {
                const ObjectHandle handle = target == OriginTarget::Selection ? ctx.scene.selection.getActiveObject() : originCommandObject(ctx);
                if (const Object* object = ctx.scene.objects.tryGet(handle)) {
                    const Vec3 p = ctx.scene.objects.worldTransform(handle).position;
                    std::cout << "[origin] " << object->name << " at " << p.x << " " << p.y << " " << p.z << std::endl;
                }
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "import",
        "Adds a .vlmobj asset as a new object, picking files if no path is given: import [<path>]",
        [&ctx](const CommandArgs& args) {
            if (!canUseProjectFiles(ctx)) return;
            if (args.empty()) {
                importAssets(ctx);
                return;
            }

            // Like project paths, but relative ones start in the export folder, where assets usually are
            std::filesystem::path path = pathFromArgs(args);
            if (!path.has_extension()) path += AssetFile::EXTENSION;
            if (path.is_relative()) path = exportFolder(ctx) / path;
            importAssetFrom(ctx, path);
        }
    );

    ctx.systems.commands.registerCommand(
        "fileinfo",
        "Lists the sections of a project file: fileinfo [<path>]",
        [&ctx](const CommandArgs& args) {
            const std::filesystem::path path = args.empty() ? ctx.project.path : resolveProjectPath(pathFromArgs(args));
            if (path.empty()) {
                ctx.systems.console.printError("This project hasn't been saved yet; give a path");
                return;
            }

            std::vector<u8> bytes;
            std::string error;
            if (!ProjectFile::readFile(path, bytes, error)) {
                ctx.systems.console.printError(error);
                return;
            }
            std::cout << ProjectFile::describe(bytes) << std::endl;
        }
    );

    ctx.systems.commands.registerCommand(
        "object",
        "Edits objects: object list | add <preset> | <id> edit | <id> remove | <id> parent <id | none> | <id> <property> <value>",
        [&ctx](const CommandArgs& args) {
            runObjectCommand(ctx, args);
        }
    );
}
