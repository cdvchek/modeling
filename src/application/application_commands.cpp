#include "application/application.hpp"

#include <iostream>

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
                    
                    ctx.scene.objects.get(0).meshData.mergeVertices(selectedVerts[0], selectedVerts[1], mergeType);
                    ctx.scene.objects.get(0).meshDirty = true;
                }
            }
        }
    );
}