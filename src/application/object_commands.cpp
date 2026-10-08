#include "application/object_commands.hpp"
#include "application/command_parsing.hpp"

#include <iostream>
#include <optional>

namespace {
    constexpr f32 DEGREES_TO_RADIANS = 3.14159265f / 180.0f;
    constexpr f32 RADIANS_TO_DEGREES = 180.0f / 3.14159265f;

    // Added objects are spaced out along X so they don't overlap the ones already there
    constexpr f32 NEW_OBJECT_SPACING = 1.5f;

    const char* USAGE =
        "usage: object list\n"
        "       object add <preset> [name]   (cube, plane, grid, circle, cylinder, cone, uvsphere, icosphere, torus)\n"
        "       object <id> [edit | remove | name <n> | parent <id | none> | position <x> <y> <z> | rotation <x> <y> <z> | scale <x> <y> <z>]\n"
        "       (position, rotation, and scale are relative to the parent)";

    const ObjectPreset* findPreset(const std::string& text) {
        for (const ObjectPreset& preset : objectPresets()) {
            if (text == preset.command) return &preset;
        }
        return nullptr;
    }

    void printVec3(const Vec3& v) {
        std::cout << v.x << " " << v.y << " " << v.z;
    }

    void printObject(const AppContext& ctx, ObjectHandle handle) {
        const Object& object = ctx.scene.objects.get(handle);
        const bool active = handle == ctx.scene.selection.getActiveObject();

        std::cout << "[object " << handle.index << "] " << object.name << (active ? " (editing)" : "")
                  << ": " << object.meshData.getVertexHandles().size() << " vertices, "
                  << object.meshData.getFaceHandles().size() << " faces, position ";
        printVec3(object.transform.position);
        std::cout << ", rotation ";
        printVec3(object.transform.rotation * RADIANS_TO_DEGREES);
        std::cout << ", scale ";
        printVec3(object.transform.scale);
        const ObjectHandle parent = ctx.scene.objects.parentOf(handle);
        if (!parent.isNull()) std::cout << ", child of " << ctx.scene.objects.get(parent).name << " (" << parent.index << ")";
        std::cout << std::endl;
    }

    void runList(const AppContext& ctx) {
        if (ctx.scene.objects.count() == 0) std::cout << "[object] no objects" << std::endl;
        for (ObjectHandle handle : ctx.scene.objects.handles()) printObject(ctx, handle);
    }

    void runAdd(AppContext& ctx, const CommandArgs& args) {
        const ObjectPreset* preset = args.size() >= 2 ? findPreset(args[1]) : nullptr;
        if (!preset || args.size() > 3) {
            std::cout << "usage: object add <preset> [name]   (cube, plane, grid, circle, cylinder, cone, uvsphere, icosphere, torus)" << std::endl;
            return;
        }

        const std::string name = args.size() == 3 ? args[2] : ctx.scene.objects.uniqueName(preset->displayName);
        const ObjectHandle handle = addObject(ctx, preset->preset, name);

        printObject(ctx, handle);
    }

    // Applies "object <id> <property> ..." to the object; prints usage and returns false on bad input
    bool editObject(Object& object, const CommandArgs& args) {
        const std::string& property = args[1];

        if (property == "name" && args.size() == 3) {
            object.name = args[2];
        } else if (property == "position") {
            if (!parseVec3Args(args, 2, object.transform.position, false)) {
                std::cout << "usage: object <id> position <x> <y> <z>" << std::endl;
                return false;
            }
        } else if (property == "rotation") {
            Vec3 degrees;
            if (!parseVec3Args(args, 2, degrees, false)) {
                std::cout << "usage: object <id> rotation <x> <y> <z>  (degrees)" << std::endl;
                return false;
            }
            object.transform.rotation = degrees * DEGREES_TO_RADIANS;
        } else if (property == "scale") {
            Vec3 scale;
            if (!parseVec3Args(args, 2, scale, false) || scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f) {
                std::cout << "usage: object <id> scale <x> <y> <z>  (none zero)" << std::endl;
                return false;
            }
            object.transform.scale = scale;
        } else {
            std::cout << USAGE << std::endl;
            return false;
        }

        return true;
    }

    void runById(AppContext& ctx, const CommandArgs& args) {
        ObjectCollection& objects = ctx.scene.objects;

        u32 slot = 0;
        const ObjectHandle handle = parseU32(args[0], slot) ? objects.handleAt(slot) : INVALID_OBJECT;
        if (!objects.isValid(handle)) {
            std::cout << "object: no object with id '" << args[0] << "' (see object list)" << std::endl;
            return;
        }

        if (args.size() == 1) {
            printObject(ctx, handle);
            return;
        }

        if (args[1] == "remove" && args.size() == 2) {
            removeObject(ctx, handle);
            std::cout << "[object " << handle.index << "] removed" << std::endl;
            return;
        }

        if (args[1] == "parent" && args.size() == 3) {
            u32 parentSlot = 0;
            const ObjectHandle parent = args[2] == "none" ? INVALID_OBJECT : parseU32(args[2], parentSlot) ? objects.handleAt(parentSlot) : INVALID_OBJECT;
            if (args[2] != "none" && !objects.isValid(parent)) {
                std::cout << "usage: object <id> parent <id | none>" << std::endl;
                return;
            }
            if (setObjectParent(ctx, handle, parent)) printObject(ctx, handle);
            return;
        }

        if (args[1] == "edit" && args.size() == 2) {
            ctx.scene.selection.clearLights();
            ctx.scene.selection.setActiveObject(handle);
            printObject(ctx, handle);
            return;
        }

        Object edited = objects.get(handle);
        if (!editObject(edited, args)) return;

        ctx.history.begin(ctx.scene);
        Object& object = objects.get(handle);
        object.name = edited.name;
        object.transform = edited.transform;
        ctx.history.commit();

        printObject(ctx, handle);
    }
}

const std::vector<ObjectPreset>& objectPresets() {
    static const std::vector<ObjectPreset> presets = {
        { "cube", "Cube", PresetMesh::Cube },
        { "plane", "Plane", PresetMesh::Plane },
        { "grid", "Grid", PresetMesh::Grid },
        { "circle", "Circle", PresetMesh::Circle },
        { "cylinder", "Cylinder", PresetMesh::Cylinder },
        { "cone", "Cone", PresetMesh::Cone },
        { "uvsphere", "UV Sphere", PresetMesh::UVSphere },
        { "icosphere", "Ico Sphere", PresetMesh::IcoSphere },
        { "torus", "Torus", PresetMesh::Torus },
    };
    return presets;
}

ObjectHandle addObject(AppContext& ctx, PresetMesh preset, const std::string& name) {
    Object object;
    object.name = name;
    object.meshData.setMesh(preset);
    return addObject(ctx, std::move(object));
}

ObjectHandle addObject(AppContext& ctx, Object object) {
    ctx.history.begin(ctx.scene);
    const ObjectHandle handle = placeNewObject(ctx, std::move(object));
    ctx.history.commit();
    return handle;
}

ObjectHandle placeNewObject(AppContext& ctx, Object object) {
    ObjectCollection& objects = ctx.scene.objects;
    object.transform.position = Vec3(NEW_OBJECT_SPACING * static_cast<f32>(objects.count()), 0.0f, 0.0f);
    object.parent = INVALID_OBJECT;

    const ObjectHandle handle = objects.add(std::move(object));
    ctx.scene.selection.clearLights();
    ctx.scene.selection.setActiveObject(handle);

    // In object mode the new object becomes the selection
    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionObject) {
        ctx.scene.selection.clearObjects();
        ctx.scene.selection.selectObject(handle);
    }
    return handle;
}

bool setObjectParent(AppContext& ctx, ObjectHandle child, ObjectHandle parent) {
    ObjectCollection& objects = ctx.scene.objects;
    if (!objects.isValid(child)) return false;

    ctx.history.begin(ctx.scene);
    if (!objects.setParent(child, parent)) {
        ctx.history.cancel(ctx.scene);
        ctx.systems.console.printError("parent: " + objects.get(child).name + " can't be a child of itself or of its own child");
        return false;
    }
    ctx.history.commit();

    const std::string& name = objects.get(child).name;
    ctx.systems.console.print(parent.isNull() ? name + " has no parent now" : name + " is now a child of " + objects.get(parent).name);
    return true;
}

void removeObject(AppContext& ctx, ObjectHandle handle) {
    ObjectCollection& objects = ctx.scene.objects;
    Selection& selection = ctx.scene.selection;
    if (!objects.isValid(handle)) return;

    ctx.history.begin(ctx.scene);
    objects.remove(handle);
    selection.deselectObject(handle);
    if (selection.getActiveObject() == handle) {
        const std::vector<ObjectHandle> remaining = objects.handles();
        selection.setActiveObject(remaining.empty() ? INVALID_OBJECT : remaining.front());
    }
    ctx.history.commit();
}

void runObjectCommand(AppContext& ctx, const CommandArgs& args) {
    if (args.empty()) {
        std::cout << USAGE << std::endl;
    } else if (args[0] == "list") {
        runList(ctx);
    } else if (args[0] == "add") {
        runAdd(ctx, args);
    } else {
        runById(ctx, args);
    }
}
