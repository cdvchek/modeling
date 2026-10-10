#include "application/actions/editing_actions.hpp"
#include "application/commands/light_commands.hpp"
#include "application/actions/origin_actions.hpp"
#include "application/viewport/reference_images.hpp"

#include <iostream>
#include <vector>

namespace {
    constexpr u32 AXIS_CONTEXTS = InputContext_XAxis | InputContext_YAxis | InputContext_ZAxis;

    void saveVertexStarts(AppContext& ctx) {
        std::vector<Vec3> starts;
        for (const auto handle : ctx.scene.selection.getVertexHandles()) {
            starts.push_back(ctx.scene.activeObject()->meshData.getVertexPosition(handle));
        }
        ctx.scene.selection.setSelectionStartPositions(starts);
    }

    // Undo can bring back a selection made in another mode; keep only what this mode can show
    void dropOtherModeSelection(AppContext& ctx) {
        if (ctx.systems.input_ctx.getModeContext() == InputContext_SelectionObject) ctx.scene.selection.clearMeshElements();
        else ctx.scene.selection.clearObjects();
    }

    void saveObjectStarts(AppContext& ctx) {
        std::vector<Transform> starts;
        for (ObjectHandle handle : ctx.scene.selection.getObjects()) {
            starts.push_back(ctx.scene.objects.worldTransform(handle));
        }
        ctx.scene.selection.setObjectStartTransforms(starts);
    }

    // Reference images keep their size in scale.y
    void saveReferenceStarts(AppContext& ctx) {
        std::vector<Transform> starts;
        for (ReferenceHandle handle : ctx.scene.selection.getReferences()) {
            Transform start;
            if (const ReferenceImage* image = ctx.scene.references.tryGet(handle)) {
                start.position = image->position;
                start.rotation = image->rotation;
                start.scale = Vec3(image->size);
            }
            starts.push_back(start);
        }
        ctx.scene.selection.setReferenceStartTransforms(starts);
    }

}

void setSelectionMode(AppContext& ctx, u32 mode) {
    Selection& selection = ctx.scene.selection;
    if (mode & InputContext_EditModes) ctx.lastEditMode = mode;

    ctx.systems.input_ctx.setModeContext(mode);
    selection.clear();
    // Any mode change leaves island mode; picking it sets it again after this
    ctx.workspace.uvIslands = false;

    // Object mode starts with the object you were editing selected
    if (mode == InputContext_SelectionObject && ctx.scene.objects.isValid(selection.getActiveObject())) {
        selection.selectObject(selection.getActiveObject());
    }
}

void toggleObjectMode(AppContext& ctx) {
    const bool inObjectMode = ctx.systems.input_ctx.getModeContext() == InputContext_SelectionObject;
    setSelectionMode(ctx, inObjectMode ? ctx.lastEditMode : InputContext_SelectionObject);
}

void toggleAxis(AppContext& ctx, u32 axis) {
    ctx.systems.input_ctx.toggleContext(axis);
}

void undo(AppContext& ctx) {
    ctx.history.undo(ctx.scene);
    dropOtherModeSelection(ctx);
}

void redo(AppContext& ctx) {
    ctx.history.redo(ctx.scene);
    dropOtherModeSelection(ctx);
}

bool canGrab(const AppContext& ctx) {
    const Selection& selection = ctx.scene.selection;
    return selection.hasVertices() || selection.hasLights() || selection.hasObjects() || selection.hasOrigin() || selection.hasReferences();
}

void startGrab(AppContext& ctx) {
    ctx.systems.input_ctx.removeContext(AXIS_CONTEXTS);
    saveVertexStarts(ctx);
    saveObjectStarts(ctx);

    std::vector<Vec3> lightStarts;
    for (LightHandle light : ctx.scene.selection.getLights()) {
        const Light* data = ctx.scene.lights.tryGet(light);
        lightStarts.push_back(data ? data->position : Vec3());
    }
    ctx.scene.selection.setLightStartPositions(lightStarts);
    saveReferenceStarts(ctx);
    beginOriginEdit(ctx);

    ctx.history.begin(ctx.scene);
    ctx.systems.input_ctx.setContext(InputContext_Grab);
}

bool canScale(const AppContext& ctx) {
    return ctx.scene.selection.getVertices().size() >= 2 || ctx.scene.selection.hasObjects() || ctx.scene.selection.hasReferences();
}

void startScale(AppContext& ctx) {
    ctx.systems.input_ctx.removeContext(AXIS_CONTEXTS);
    saveVertexStarts(ctx);
    saveObjectStarts(ctx);
    saveReferenceStarts(ctx);
    ctx.history.begin(ctx.scene);
    ctx.transformTool = {};
    ctx.systems.input_ctx.setContext(InputContext_Scale);
}

bool canRotate(const AppContext& ctx) {
    const Selection& selection = ctx.scene.selection;
    return selection.getVertices().size() >= 2 || selection.hasLights() || selection.hasObjects() || selection.hasOrigin() || selection.hasReferences();
}

void startRotate(AppContext& ctx) {
    ctx.systems.input_ctx.removeContext(AXIS_CONTEXTS);
    saveVertexStarts(ctx);
    saveObjectStarts(ctx);

    std::vector<Vec3> lightDirections;
    for (LightHandle light : ctx.scene.selection.getLights()) {
        const Light* data = ctx.scene.lights.tryGet(light);
        lightDirections.push_back(data ? data->direction : Vec3(0.0f, -1.0f, 0.0f));
    }
    ctx.scene.selection.setLightStartDirections(lightDirections);
    saveReferenceStarts(ctx);
    beginOriginEdit(ctx);

    ctx.history.begin(ctx.scene);
    ctx.transformTool = {};
    ctx.systems.input_ctx.setContext(InputContext_Rotate);
}

bool canBevel(const AppContext& ctx) {
    const Selection& selection = ctx.scene.selection;
    const u32 mode = ctx.systems.input_ctx.getModeContext();

    if (mode == InputContext_SelectionVertex) return selection.getVertices().size() == 1;
    if (mode == InputContext_SelectionEdge) return selection.getEdges().size() == 1;
    if (mode == InputContext_SelectionFace) return selection.getFaces().size() == 1;
    return false;
}

bool canExtrude(const AppContext& ctx) {
    return ctx.systems.input_ctx.getModeContext() == InputContext_SelectionFace && !ctx.scene.selection.getFaceHandles().empty() && ctx.scene.activeObject();
}

void extrudeSelection(AppContext& ctx) {
    Selection& selection = ctx.scene.selection;
    const ObjectHandle handle = selection.getActiveObject();
    Object* object = ctx.scene.activeObject();

    // Every selected region gets walls; the grab that follows moves all the tops together
    ctx.history.begin(ctx.scene);
    std::vector<FaceHandle> tops;
    const RegionError error = object->meshData.extrudeRegions(selection.getFaceHandles(), tops);

    if (error != RegionError::None) {
        ctx.history.cancel(ctx.scene);
        ctx.systems.console.printError(std::string("extrude: ") + regionErrorText(error));
        return;
    }

    selection.clear();
    for (FaceHandle top : tops) {
        selection.addFace(handle, top);
        for (VertexHandle vertex : object->meshData.getFaceVertices(top)) {
            if (!selection.hasVertex(handle, vertex)) selection.addVertex(handle, vertex);
        }
    }

    saveVertexStarts(ctx);
    object->meshDirty = true;
    ctx.systems.input_ctx.setContext(InputContext_Grab);
}

bool canConnectVertices(const AppContext& ctx) {
    return ctx.systems.input_ctx.getModeContext() == InputContext_SelectionVertex && ctx.scene.selection.getVertices().size() == 2;
}

void connectVertices(AppContext& ctx) {
    const auto vertHandles = ctx.scene.selection.getVertexHandles();
    ctx.history.begin(ctx.scene);

    if (ctx.scene.activeObject()->meshData.connectVertices(vertHandles[0], vertHandles[1])) ctx.history.commit();
    else ctx.history.cancel(ctx.scene);

    ctx.scene.activeObject()->meshDirty = true;
}

bool canFillFaceLoop(const AppContext& ctx) {
    return ctx.systems.input_ctx.getModeContext() == InputContext_SelectionEdge && !ctx.scene.selection.getEdgeHandles().empty();
}

void fillFaceLoop(AppContext& ctx) {
    ctx.history.begin(ctx.scene);

    if (ctx.scene.activeObject()->meshData.fillFaceLoop(ctx.scene.selection.getEdgeHandles()[0])) ctx.history.commit();
    else ctx.history.cancel(ctx.scene);

    ctx.scene.activeObject()->meshDirty = true;
}

bool canMergeVertices(const AppContext& ctx) {
    return ctx.systems.input_ctx.isActive(InputContext_SelectionVertex) && ctx.scene.selection.getVertices().size() == 2;
}

void mergeVertices(AppContext& ctx, u8 mergeType) {
    const std::vector<VertexHandle> verts = ctx.scene.selection.getVertexHandles();
    ctx.history.begin(ctx.scene);

    if (ctx.scene.activeObject()->meshData.mergeVertices(verts[0], verts[1], mergeType)) {
        ctx.scene.selection.removeVertex(ctx.scene.selection.getActiveObject(), verts[1]);
        ctx.history.commit();
    } else {
        ctx.history.cancel(ctx.scene);
    }

    ctx.scene.activeObject()->meshDirty = true;
}

bool canDissolve(const AppContext& ctx) {
    const u32 mode = ctx.systems.input_ctx.getModeContext();
    if (!ctx.scene.activeObject()) return false;
    if (mode == InputContext_SelectionEdge) return ctx.scene.selection.getEdgeHandles().size() == 1;
    if (mode == InputContext_SelectionFace) return ctx.scene.selection.getFaceHandles().size() == 1;
    return false;
}

void dissolveSelection(AppContext& ctx) {
    Object* object = ctx.scene.activeObject();
    MeshData& mesh = object->meshData;
    ctx.history.begin(ctx.scene);

    const bool dissolved = ctx.systems.input_ctx.getModeContext() == InputContext_SelectionEdge
        ? mesh.isValidHandle(mesh.dissolveEdge(ctx.scene.selection.getEdgeHandles()[0]))
        : mesh.isValidHandle(mesh.dissolveFace(ctx.scene.selection.getFaceHandles()[0]));

    if (!dissolved) {
        ctx.history.cancel(ctx.scene);
        ctx.systems.console.printError("dissolve: can't collapse this without breaking the mesh");
        return;
    }

    ctx.scene.selection.clear();
    object->meshDirty = true;
    ctx.history.commit();
}

bool canParentToActive(const AppContext& ctx) {
    const Selection& selection = ctx.scene.selection;
    if (ctx.systems.input_ctx.getModeContext() != InputContext_SelectionObject) return false;
    if (!ctx.scene.objects.isValid(selection.getActiveObject())) return false;
    for (ObjectHandle handle : selection.getObjects()) {
        if (handle != selection.getActiveObject()) return true;
    }
    return false;
}

void parentToActive(AppContext& ctx) {
    ObjectCollection& objects = ctx.scene.objects;
    const ObjectHandle parent = ctx.scene.selection.getActiveObject();

    ctx.history.begin(ctx.scene);
    u32 parented = 0;
    for (ObjectHandle child : ctx.scene.selection.getObjects()) {
        if (child == parent) continue;
        if (objects.setParent(child, parent)) {
            ++parented;
        } else {
            ctx.systems.console.printError("parent: " + objects.get(child).name + " can't be a child of its own child " + objects.get(parent).name);
        }
    }

    if (parented == 0) {
        ctx.history.cancel(ctx.scene);
        return;
    }
    ctx.history.commit();
    ctx.systems.console.print("Parented " + std::to_string(parented) + (parented == 1 ? " object" : " objects") + " to " + objects.get(parent).name);
}

bool canClearParents(const AppContext& ctx) {
    if (ctx.systems.input_ctx.getModeContext() != InputContext_SelectionObject) return false;
    for (ObjectHandle handle : ctx.scene.selection.getObjects()) {
        if (!ctx.scene.objects.parentOf(handle).isNull()) return true;
    }
    return false;
}

void clearParents(AppContext& ctx) {
    ctx.history.begin(ctx.scene);
    for (ObjectHandle handle : ctx.scene.selection.getObjects()) ctx.scene.objects.setParent(handle, INVALID_OBJECT);
    ctx.history.commit();
}

bool canDelete(const AppContext& ctx) {
    return ctx.scene.selection.hasLights() || ctx.scene.selection.hasVertices() || ctx.scene.selection.hasObjects() || ctx.scene.selection.hasReferences();
}

void deleteSelectedObjects(AppContext& ctx) {
    ObjectCollection& objects = ctx.scene.objects;
    Selection& selection = ctx.scene.selection;

    ctx.history.begin(ctx.scene);

    const bool removingActive = selection.hasObject(selection.getActiveObject());
    for (ObjectHandle handle : selection.getObjects()) {
        if (objects.isValid(handle)) objects.remove(handle);
    }
    selection.clearObjects();

    // The next object to edit is the first one left
    if (removingActive) {
        const std::vector<ObjectHandle> remaining = objects.handles();
        selection.setActiveObject(remaining.empty() ? INVALID_OBJECT : remaining.front());
    }

    ctx.history.commit();
}

void deleteSelection(AppContext& ctx) {
    if (ctx.scene.selection.hasReferences()) {
        deleteSelectedReferences(ctx);
        return;
    }

    if (ctx.scene.selection.hasObjects()) {
        deleteSelectedObjects(ctx);
        return;
    }

    if (ctx.scene.selection.hasLights()) deleteSelectedLights(ctx);
    if (!ctx.scene.selection.hasVertices()) return;

    ctx.history.begin(ctx.scene);
    MeshData& mesh = ctx.scene.activeObject()->meshData;
    const u32 mode = ctx.systems.input_ctx.getModeContext();

    if (mode == InputContext_SelectionVertex) {
        for (VertexHandle vert : ctx.scene.selection.getVertexHandles()) mesh.removeVertex(vert);
    } else if (mode == InputContext_SelectionEdge) {
        for (EdgeHandle edge : ctx.scene.selection.getEdgeHandles()) mesh.removeEdge(edge);
    } else {
        for (FaceHandle face : ctx.scene.selection.getFaceHandles()) mesh.removeFace(face);
    }

    ctx.scene.activeObject()->meshDirty = true;
    ctx.scene.selection.clear();
    ctx.history.commit();
}

bool canChangeAxis(const AppContext& ctx) {
    return ctx.systems.input_ctx.isActive(InputContext_Grab | InputContext_Scale | InputContext_Rotate);
}

void clearAxes(AppContext& ctx) {
    ctx.systems.input_ctx.removeContext(AXIS_CONTEXTS);
}

bool canSetLightType(const AppContext& ctx, LightType type) {
    // Dimmed when every selected light already has this type
    for (LightHandle handle : ctx.scene.selection.getLights()) {
        const Light* light = ctx.scene.lights.tryGet(handle);
        if (light && light->type != type) return true;
    }
    return false;
}

void setLightType(AppContext& ctx, LightType type) {
    ctx.history.begin(ctx.scene);
    for (LightHandle handle : ctx.scene.selection.getLights()) {
        if (Light* light = ctx.scene.lights.tryGet(handle)) light->type = type;
    }
    ctx.history.commit();
}

bool canToggleLights(const AppContext& ctx) {
    return ctx.scene.selection.hasLights();
}

void toggleLights(AppContext& ctx) {
    // Turns them all off if all are on, otherwise all on
    bool allOn = true;
    for (LightHandle handle : ctx.scene.selection.getLights()) {
        const Light* light = ctx.scene.lights.tryGet(handle);
        if (light && !light->enabled) allOn = false;
    }

    ctx.history.begin(ctx.scene);
    for (LightHandle handle : ctx.scene.selection.getLights()) {
        if (Light* light = ctx.scene.lights.tryGet(handle)) light->enabled = !allOn;
    }
    ctx.history.commit();
}

void togglePanel(AppContext& ctx) {
    ctx.viewport.showPanel = !ctx.viewport.showPanel;
}

void toggleHeadlight(AppContext& ctx) {
    ctx.viewport.headlight.enabled = !ctx.viewport.headlight.enabled;
}

void toggleDebugView(AppContext& ctx) {
    ctx.systems.input_ctx.toggleContext(InputContext_Debug);
}

void toggleMaterials(AppContext& ctx) {
    ctx.viewport.showMaterials = !ctx.viewport.showMaterials;
}

void toggleUVChecker(AppContext& ctx) {
    ctx.viewport.showUVChecker = !ctx.viewport.showUVChecker;
}
