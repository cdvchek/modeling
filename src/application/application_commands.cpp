#include "application/application.hpp"

#include <iostream>
#include <cstdlib>

namespace {
    bool parseUnitFloat(const std::string& text, f32& out) {
        char* end = nullptr;
        const f32 value = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || end != text.c_str() + text.size()) return false;
        if (!(value >= 0.0f && value <= 1.0f)) return false;

        out = value;
        return true;
    }

    void printAmbient(const AmbientLight& ambient) {
        std::cout << "[light ambient] color "
                  << ambient.color.x << " " << ambient.color.y << " " << ambient.color.z
                  << ", strength " << ambient.strength << std::endl;
    }

    void runLightAmbientCommand(AppContext& ctx, const CommandArgs& args) {
        LightCollection& lights = ctx.scene.lights;

        if (args.size() == 1) {
            printAmbient(lights.getAmbient());
            return;
        }

        const std::string& property = args[1];

        if (property == "color") {
            Vec3 color;
            if (args.size() != 5
                || !parseUnitFloat(args[2], color.x)
                || !parseUnitFloat(args[3], color.y)
                || !parseUnitFloat(args[4], color.z)) {
                std::cout << "usage: light ambient color <r> <g> <b>  (each 0 to 1)" << std::endl;
                return;
            }

            ctx.history.begin(ctx.scene);
            lights.setAmbientColor(color);
            ctx.history.commit();
        } else if (property == "strength") {
            f32 strength = 0.0f;
            if (args.size() != 3 || !parseUnitFloat(args[2], strength)) {
                std::cout << "usage: light ambient strength <0 to 1>" << std::endl;
                return;
            }

            ctx.history.begin(ctx.scene);
            lights.setAmbientStrength(strength);
            ctx.history.commit();
        } else {
            std::cout << "light ambient: unknown property '" << property << "' (color, strength)" << std::endl;
            return;
        }

        printAmbient(lights.getAmbient());
    }

    void printHeadlight(const Headlight& headlight) {
        std::cout << "[headlight] " << (headlight.enabled ? "on" : "off")
                  << ", color " << headlight.color.x << " " << headlight.color.y << " " << headlight.color.z
                  << ", strength " << headlight.strength << std::endl;
    }

    void printBackFaceTint(const Vec3& tint) {
        std::cout << "[backface tint] " << tint.x << " " << tint.y << " " << tint.z << std::endl;
    }
}

void Application::registerCommands(AppContext& ctx) {
    ctx.systems.commands.registerCommand(
        "test",
        "This is a test command to make sure everything works",
        [&ctx](const CommandArgs& args) {
            for (auto arg : args) {
                std::cout << arg << std::endl;
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "debug",
        "Toggles or sets the state of debug visuals",
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
        "Checks the mesh's half-edge structure and reports the first problem.",
        [&ctx](const CommandArgs& args) {
            const MeshData& mesh = ctx.scene.objects.get(0).meshData;

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
        "Merges selected vertices into one vertex.",
        [&ctx](const CommandArgs& args) {
            if (ctx.systems.input_ctx.isActive(InputContext_SelectionVertex)) {
                const std::vector<VertexHandle>& selectedVerts = ctx.scene.selection.getVertexHandles();
                if (selectedVerts.size() == 2) {
                    u8 mergeType = 0;
                    if (!args.empty()) {
                        if (args[0] == "center") {
                            mergeType = 0;
                        }
                        else if (args[0] == "first") {
                            mergeType = 1;
                        } else if (args[0] == "last") {
                            mergeType = 2;
                        }
                    }
                    
                    const VertexHandle a = selectedVerts[0];
                    const VertexHandle b = selectedVerts[1];

                    ctx.history.begin(ctx.scene);

                    if (ctx.scene.objects.get(0).meshData.mergeVertices(a, b, mergeType)) {
                        ctx.scene.selection.removeVertex(0, b);
                        ctx.history.commit();
                    } else {
                        ctx.history.cancel(ctx.scene);
                    }

                    ctx.scene.objects.get(0).meshDirty = true;
                }
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "dissolve",
        "Collapses the selected edge or face into a single vertex.",
        [&ctx](const CommandArgs& args) {
            MeshData& mesh = ctx.scene.objects.get(0).meshData;
            const u32 selectionContext = ctx.systems.input_ctx.getSelectionContext();

            bool dissolved = false;

            if (selectionContext == InputContext_SelectionEdge) {
                const std::vector<EdgeHandle> edges = ctx.scene.selection.getEdgeHandles();
                if (edges.size() != 1) return;

                ctx.history.begin(ctx.scene);
                dissolved = mesh.isValidHandle(mesh.dissolveEdge(edges[0]));
            } else if (selectionContext == InputContext_SelectionFace) {
                const std::vector<FaceHandle> faces = ctx.scene.selection.getFaceHandles();
                if (faces.size() != 1) return;

                ctx.history.begin(ctx.scene);
                dissolved = mesh.isValidHandle(mesh.dissolveFace(faces[0]));
            } else {
                return;
            }

            if (!dissolved) {
                ctx.history.cancel(ctx.scene);
                std::cout << "dissolve: can't collapse this without breaking the mesh" << std::endl;
                return;
            }

            ctx.scene.selection.clear();
            ctx.scene.objects.get(0).meshDirty = true;

            ctx.history.commit();
        }
    );

    ctx.systems.commands.registerCommand(
        "light",
        "Edits scene lighting: light ambient [color r g b | strength s]",
        [&ctx](const CommandArgs& args) {
            if (args.empty()) {
                std::cout << "usage: light ambient [color <r> <g> <b> | strength <s>]" << std::endl;
                return;
            }

            if (args[0] == "ambient") {
                runLightAmbientCommand(ctx, args);
            } else {
                std::cout << "light: unknown target '" << args[0] << "' (ambient)" << std::endl;
            }
        }
    );

    ctx.systems.commands.registerCommand(
        "backface",
        "Changes how back faces are drawn: backface tint [r g b]",
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
        "headlight",
        "Light that follows the camera: headlight [on | off | color r g b | strength s]",
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
}
