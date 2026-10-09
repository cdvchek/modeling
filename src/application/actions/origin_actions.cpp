#include "application/actions/origin_actions.hpp"

namespace {
    constexpr u32 TOOL_CONTEXTS = InputContext_Grab | InputContext_Scale | InputContext_Rotate | InputContext_Bevel | InputContext_Inset;

    Transform targetTransform(const AppContext& ctx, ObjectHandle handle, OriginTarget target) {
        const ObjectCollection& objects = ctx.scene.objects;
        switch (target) {
            case OriginTarget::Geometry: return originAtCenter(objects, handle);
            case OriginTarget::Bottom: return originAtBottom(objects, handle);
            case OriginTarget::World: return originAtWorld(objects, handle);
            case OriginTarget::WorldRotation: return originAlignedToWorld(objects, handle);
            case OriginTarget::Selection: return originAtVertices(objects, handle, ctx.scene.selection.getVertexHandles());
        }
        return objects.worldTransform(handle);
    }
}

ObjectHandle originCommandObject(const AppContext& ctx) {
    const Selection& selection = ctx.scene.selection;
    return selection.hasOrigin() ? selection.getOrigin() : selection.getActiveObject();
}

bool canMoveOrigin(const AppContext& ctx, OriginTarget target) {
    if (ctx.systems.input_ctx.getContext() & TOOL_CONTEXTS) return false;

    const Selection& selection = ctx.scene.selection;
    if (target == OriginTarget::Selection) {
        return (ctx.systems.input_ctx.getSelectionContext() & InputContext_EditModes) && selection.hasVertices() && ctx.scene.activeObject();
    }
    return selection.hasOrigin() && ctx.scene.objects.isValid(selection.getOrigin());
}

bool moveOrigin(AppContext& ctx, OriginTarget target) {
    const ObjectHandle handle = target == OriginTarget::Selection ? ctx.scene.selection.getActiveObject() : originCommandObject(ctx);
    Object* object = ctx.scene.objects.tryGet(handle);
    if (!object) {
        ctx.systems.console.printError("origin: there's no object to move the origin of");
        return false;
    }
    if (target == OriginTarget::Selection && !ctx.scene.selection.hasVertices()) {
        ctx.systems.console.printError("origin: select vertices, edges, or faces on the object first");
        return false;
    }

    ctx.history.begin(ctx.scene);
    setOrigin(ctx.scene.objects, handle, targetTransform(ctx, handle, target));
    ctx.history.commit();
    return true;
}

void toggleOrigins(AppContext& ctx) {
    ctx.viewport.showOrigins = !ctx.viewport.showOrigins;
    if (!ctx.viewport.showOrigins) ctx.scene.selection.clearOrigin();
}

void beginOriginEdit(AppContext& ctx) {
    OriginEdit& edit = ctx.originEdit;
    edit = {};

    if (!ctx.scene.objects.isValid(ctx.scene.selection.getOrigin())) return;

    edit.object = ctx.scene.selection.getOrigin();
    edit.start = captureOrigin(ctx.scene.objects, edit.object);
}

void updateOriginEdit(AppContext& ctx, const Transform& to) {
    const OriginEdit& edit = ctx.originEdit;
    if (ctx.scene.objects.isValid(edit.object)) setOrigin(ctx.scene.objects, edit.object, edit.start, to);
}
