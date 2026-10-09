#include "application/shading_commands.hpp"
#include "application/command_parsing.hpp"
#include "core/math/math_utils.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {
    constexpr u32 TOOL_CONTEXTS = InputContext_Grab | InputContext_Scale | InputContext_Rotate | InputContext_Bevel | InputContext_Inset;
    constexpr f32 DEGREES = 180.0f / Math::PI;

    const char* USAGE =
        "usage: shading  (lists the active object's)\n"
        "       shading flat | smooth | auto [<degrees>]  (the selected objects in object mode, else the active one)\n"
        "       shading mark hard | smooth | clear  (the selected edges, in edge mode)";

    bool toolRunning(const AppContext& ctx) {
        return ctx.systems.input_ctx.isActive(TOOL_CONTEXTS);
    }

    void printShading(const AppContext& ctx) {
        const ObjectHandle active = ctx.scene.selection.getActiveObject();
        const Object* object = ctx.scene.objects.tryGet(active);
        if (!object) {
            std::cout << "no active object" << std::endl;
            return;
        }

        const MeshData& mesh = object->meshData;
        u32 hard = 0, smooth = 0;
        for (EdgeHandle edge : mesh.getEdgeHandles()) {
            if (mesh.getEdgeMark(edge) == EdgeMark::Hard) ++hard;
            if (mesh.getEdgeMark(edge) == EdgeMark::Smooth) ++smooth;
        }

        std::cout << "[shading] " << object->name << ": " << shadingName(mesh.getShading());
        if (mesh.getShading() == ShadingMode::Auto) std::cout << " below " << std::lround(mesh.getSmoothAngle() * DEGREES) << " degrees";
        // Each mark is on both half-edges
        std::cout << ", " << hard / 2 << " edges marked hard, " << smooth / 2 << " marked smooth" << std::endl;
    }
}

const char* shadingName(ShadingMode mode) {
    switch (mode) {
        case ShadingMode::Flat: return "flat";
        case ShadingMode::Smooth: return "smooth";
        case ShadingMode::Auto: return "auto";
    }
    return "";
}

std::vector<ObjectHandle> shadingTargets(const AppContext& ctx) {
    if (toolRunning(ctx)) return {};
    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionObject) return ctx.scene.selection.getObjects();

    const ObjectHandle active = ctx.scene.selection.getActiveObject();
    if (ctx.scene.objects.isValid(active)) return { active };
    return {};
}

bool canSetShading(const AppContext& ctx) {
    return !shadingTargets(ctx).empty();
}

void setShading(AppContext& ctx, ShadingMode mode) {
    const std::vector<ObjectHandle> targets = shadingTargets(ctx);
    if (targets.empty()) return;

    ctx.history.begin(ctx.scene);
    for (ObjectHandle handle : targets) {
        if (Object* object = ctx.scene.objects.tryGet(handle)) {
            object->meshData.setShading(mode);
            object->meshDirty = true;
        }
    }
    ctx.history.commit();
}

bool canMarkEdges(const AppContext& ctx) {
    return !toolRunning(ctx) && ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionEdge && ctx.scene.selection.hasEdges()
        && ctx.scene.objects.isValid(ctx.scene.selection.getActiveObject());
}

void markSelectedEdges(AppContext& ctx, EdgeMark mark) {
    if (!canMarkEdges(ctx)) return;
    Object& object = ctx.scene.objects.get(ctx.scene.selection.getActiveObject());

    ctx.history.begin(ctx.scene);
    for (EdgeHandle edge : ctx.scene.selection.getEdgeHandles()) object.meshData.setEdgeMark(edge, mark);
    object.meshDirty = true;
    ctx.history.commit();
}

void runShadingCommand(AppContext& ctx, const CommandArgs& args) {
    if (args.empty()) {
        printShading(ctx);
        return;
    }

    if (args[0] == "mark") {
        EdgeMark mark = EdgeMark::None;
        if (args.size() == 2 && args[1] == "hard") mark = EdgeMark::Hard;
        else if (args.size() == 2 && args[1] == "smooth") mark = EdgeMark::Smooth;
        else if (!(args.size() == 2 && args[1] == "clear")) {
            std::cout << USAGE << std::endl;
            return;
        }
        if (!canMarkEdges(ctx)) {
            ctx.systems.console.printError("shading mark: select edges in edge mode first");
            return;
        }
        const std::size_t count = ctx.scene.selection.getEdgeHandles().size();
        markSelectedEdges(ctx, mark);
        std::cout << "[shading] " << count << (count == 1 ? " edge " : " edges ") << (mark == EdgeMark::None ? "cleared" : mark == EdgeMark::Hard ? "marked hard" : "marked smooth") << std::endl;
        return;
    }

    ShadingMode mode;
    if (args[0] == "flat" && args.size() == 1) mode = ShadingMode::Flat;
    else if (args[0] == "smooth" && args.size() == 1) mode = ShadingMode::Smooth;
    else if (args[0] == "auto" && args.size() <= 2) mode = ShadingMode::Auto;
    else {
        std::cout << USAGE << std::endl;
        return;
    }

    f32 degrees = -1.0f;
    if (args.size() == 2 && (!parseFloat(args[1], degrees) || degrees < 0.0f || degrees > 180.0f)) {
        std::cout << "usage: shading auto [<degrees>]  (0 to 180)" << std::endl;
        return;
    }

    const std::vector<ObjectHandle> targets = shadingTargets(ctx);
    if (targets.empty()) {
        ctx.systems.console.printError("shading: no object to shade");
        return;
    }

    // The mode and angle together are one undo step
    ctx.history.begin(ctx.scene);
    for (ObjectHandle handle : targets) {
        Object& object = ctx.scene.objects.get(handle);
        object.meshData.setShading(mode);
        if (degrees >= 0.0f) object.meshData.setSmoothAngle(degrees / DEGREES);
        object.meshDirty = true;
    }
    ctx.history.commit();
    printShading(ctx);
}
