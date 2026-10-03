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
        "validate",
        "Checks the mesh's half-edge structure and reports the first problem.",
        [&ctx](const CommandArgs& args) {
            if (ctx.scene.objects.get(0).meshData.validate()) {
                std::cout << "[mesh validate] ok" << std::endl;
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

                    // mergeVertices keeps a and deletes b.
                    if (ctx.scene.objects.get(0).meshData.mergeVertices(a, b, mergeType)) {
                        ctx.scene.selection.removeVertex(0, b);
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

                dissolved = mesh.isValidHandle(mesh.dissolveEdge(edges[0]));
            } else if (selectionContext == InputContext_SelectionFace) {
                const std::vector<FaceHandle> faces = ctx.scene.selection.getFaceHandles();
                if (faces.size() != 1) return;

                dissolved = mesh.isValidHandle(mesh.dissolveFace(faces[0]));
            } else {
                return;
            }

            if (!dissolved) {
                std::cout << "dissolve: can't collapse this without breaking the mesh" << std::endl;
                return;
            }

            // The selected edge or face no longer exists.
            ctx.scene.selection.clear();
            ctx.scene.objects.get(0).meshDirty = true;
        }
    );
}