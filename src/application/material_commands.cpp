#include "application/material_commands.hpp"
#include "application/command_parsing.hpp"

#include <iostream>
#include <string>
#include <optional>

namespace {
    const char* USAGE =
        "usage: material list\n"
        "       material add [name]\n"
        "       material clear  (the selected faces use their object's material again)\n"
        "       material <id> [remove | assign [<object id> ...] | select | name <n> | color <r> <g> <b> | roughness <0 to 1>\n"
        "                      | metallic <0 to 1> | emissive <r> <g> <b> | glow <strength> | opacity <0 to 1>\n"
        "                      | mode <opaque | cutout | blend> | cutoff <0 to 1> | sides <single | double>]";

    std::optional<AlphaMode> parseAlphaMode(const std::string& text) {
        if (text == "opaque") return AlphaMode::Opaque;
        if (text == "cutout") return AlphaMode::Cutout;
        if (text == "blend") return AlphaMode::Blend;
        return std::nullopt;
    }

    void printVec3(const Vec3& v) {
        std::cout << v.x << " " << v.y << " " << v.z;
    }

    void printMaterial(const AppContext& ctx, MaterialHandle handle) {
        const Material& material = ctx.scene.materials.get(handle);

        std::cout << "[material " << handle.index << "] " << material.name << (ctx.scene.materials.isDefault(handle) ? " (default)" : "")
                  << ": color ";
        printVec3(material.baseColor);
        std::cout << ", roughness " << material.roughness << ", metallic " << material.metallic;
        if (material.emissiveStrength > 0.0f) {
            std::cout << ", emissive ";
            printVec3(material.emissiveColor);
            std::cout << " x " << material.emissiveStrength;
        }
        std::cout << ", " << alphaModeName(material.alphaMode);
        if (material.alphaMode != AlphaMode::Opaque) std::cout << " opacity " << material.opacity;
        if (material.alphaMode == AlphaMode::Cutout) std::cout << " cutoff " << material.alphaCutoff;
        std::cout << ", " << (material.doubleSided ? "double" : "single") << "-sided, used by "
                  << describeUse(materialUse(ctx, handle)) << std::endl;
    }

    // Applies "material <id> <property> ..." to the material; prints usage and returns false on bad input
    bool editMaterial(Material& material, const CommandArgs& args) {
        const std::string& property = args[1];
        f32 value = 0.0f;

        if (property == "name" && args.size() == 3) {
            material.name = args[2];
        } else if (property == "color") {
            if (!parseVec3Args(args, 2, material.baseColor, true)) {
                std::cout << "usage: material <id> color <r> <g> <b>  (each 0 to 1)" << std::endl;
                return false;
            }
        } else if (property == "emissive") {
            if (!parseVec3Args(args, 2, material.emissiveColor, true)) {
                std::cout << "usage: material <id> emissive <r> <g> <b>  (each 0 to 1)" << std::endl;
                return false;
            }
        } else if (property == "glow") {
            if (args.size() != 3 || !parseFloat(args[2], value) || value < 0.0f) {
                std::cout << "usage: material <id> glow <strength>  (0 or more; 0 is none)" << std::endl;
                return false;
            }
            material.emissiveStrength = value;
        } else if (property == "roughness" || property == "metallic" || property == "opacity" || property == "cutoff") {
            if (args.size() != 3 || !parseUnitFloat(args[2], value)) {
                std::cout << "usage: material <id> " << property << " <0 to 1>" << std::endl;
                return false;
            }
            if (property == "roughness") material.roughness = value;
            else if (property == "metallic") material.metallic = value;
            else if (property == "opacity") material.opacity = value;
            else material.alphaCutoff = value;
        } else if (property == "mode") {
            const std::optional<AlphaMode> mode = args.size() == 3 ? parseAlphaMode(args[2]) : std::nullopt;
            if (!mode) {
                std::cout << "usage: material <id> mode <opaque | cutout | blend>" << std::endl;
                return false;
            }
            material.alphaMode = *mode;
        } else if (property == "sides") {
            if (args.size() != 3 || (args[2] != "single" && args[2] != "double")) {
                std::cout << "usage: material <id> sides <single | double>" << std::endl;
                return false;
            }
            material.doubleSided = args[2] == "double";
        } else {
            std::cout << USAGE << std::endl;
            return false;
        }

        return true;
    }

    void runAssign(AppContext& ctx, MaterialHandle handle, const CommandArgs& args) {
        std::vector<ObjectHandle> targets;
        for (std::size_t i = 2; i < args.size(); ++i) {
            u32 slot = 0;
            const ObjectHandle object = parseU32(args[i], slot) ? ctx.scene.objects.handleAt(slot) : INVALID_OBJECT;
            if (!ctx.scene.objects.isValid(object)) {
                std::cout << "material: no object with id '" << args[i] << "' (see object list)" << std::endl;
                return;
            }
            targets.push_back(object);
        }
        // Without ids, selected faces in face mode take it; otherwise the selected objects
        if (args.size() == 2 && assignsToFaces(ctx)) {
            const std::size_t count = ctx.scene.selection.getFaces().size();
            assignFaceMaterial(ctx, handle);
            std::cout << "[material " << handle.index << "] " << ctx.scene.materials.get(handle).name << " assigned to " << count
                      << (count == 1 ? " face" : " faces") << std::endl;
            return;
        }
        if (args.size() == 2) targets = materialTargets(ctx);

        if (targets.empty()) {
            std::cout << "material: no objects to assign to (select some, or give their ids)" << std::endl;
            return;
        }

        assignMaterial(ctx, targets, handle);
        std::cout << "[material " << handle.index << "] " << ctx.scene.materials.get(handle).name << " assigned to " << targets.size()
                  << (targets.size() == 1 ? " object" : " objects") << std::endl;
    }

    void runMaterialById(AppContext& ctx, const CommandArgs& args) {
        MaterialCollection& materials = ctx.scene.materials;

        u32 slot = 0;
        const MaterialHandle handle = parseU32(args[0], slot) ? materials.handleAt(slot) : INVALID_MATERIAL;
        if (!materials.isValid(handle)) {
            std::cout << "material: no material with id '" << args[0] << "' (see material list)" << std::endl;
            return;
        }

        if (args.size() == 1) {
            printMaterial(ctx, handle);
            return;
        }

        if (args[1] == "assign") {
            runAssign(ctx, handle, args);
            return;
        }

        if (args[1] == "select" && args.size() == 2) {
            if (ctx.systems.input_ctx.getSelectionContext() != InputContext_SelectionFace) {
                std::cout << "material: select works in face mode" << std::endl;
                return;
            }
            selectFacesWithMaterial(ctx, handle);
            std::cout << "[material " << handle.index << "] " << ctx.scene.selection.getFaces().size() << " faces selected" << std::endl;
            return;
        }

        if (args[1] == "remove" && args.size() == 2) {
            if (!removeMaterial(ctx, handle)) {
                std::cout << "material: Default can't be removed" << std::endl;
                return;
            }
            std::cout << "[material " << handle.index << "] removed; its objects use Default and its faces their object's" << std::endl;
            return;
        }

        Material edited = materials.get(handle);
        if (!editMaterial(edited, args)) return;

        ctx.history.begin(ctx.scene);
        materials.get(handle) = edited;
        ctx.history.commit();

        printMaterial(ctx, handle);
    }
}

std::vector<ObjectHandle> materialTargets(const AppContext& ctx) {
    if (ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionObject) return ctx.scene.selection.getObjects();

    const ObjectHandle active = ctx.scene.selection.getActiveObject();
    if (ctx.scene.objects.isValid(active)) return { active };
    return {};
}

void assignMaterial(AppContext& ctx, const std::vector<ObjectHandle>& objects, MaterialHandle material) {
    ctx.history.begin(ctx.scene);
    for (ObjectHandle handle : objects) {
        if (Object* object = ctx.scene.objects.tryGet(handle)) object->material = material;
    }
    ctx.history.commit();
}

bool removeMaterial(AppContext& ctx, MaterialHandle material) {
    if (ctx.scene.materials.isDefault(material) || !ctx.scene.materials.isValid(material)) return false;

    ctx.history.begin(ctx.scene);
    ctx.scene.materials.remove(material);
    ctx.history.commit();

    // Faces that had it now draw with their object's material, in a different run of the mesh
    ctx.scene.objects.markAllDirty();
    return true;
}

bool assignsToFaces(const AppContext& ctx) {
    return ctx.systems.input_ctx.getSelectionContext() == InputContext_SelectionFace && ctx.scene.selection.hasFaces()
        && ctx.scene.objects.isValid(ctx.scene.selection.getActiveObject());
}

void assignFaceMaterial(AppContext& ctx, MaterialHandle material) {
    Object* object = ctx.scene.objects.tryGet(ctx.scene.selection.getActiveObject());
    if (!object) return;

    ctx.history.begin(ctx.scene);
    for (FaceHandle face : ctx.scene.selection.getFaceHandles()) object->meshData.setFaceMaterial(face, material);
    object->meshDirty = true;
    ctx.history.commit();
}

void selectFacesWithMaterial(AppContext& ctx, MaterialHandle material) {
    const ObjectHandle active = ctx.scene.selection.getActiveObject();
    const Object* object = ctx.scene.objects.tryGet(active);
    if (!object) return;

    const MaterialCollection& materials = ctx.scene.materials;
    const MaterialHandle objectMaterial = materials.resolve(object->material);
    Selection& selection = ctx.scene.selection;
    selection.clearMeshElements();

    // A face selected in face mode brings its vertices along, as a click does
    for (FaceHandle face : object->meshData.getFaceHandles()) {
        const MaterialHandle own = object->meshData.getFaceMaterial(face);
        const MaterialHandle shown = materials.isValid(own) ? own : objectMaterial;
        if (!(shown == material)) continue;

        selection.addFace(active, face);
        for (VertexHandle vertex : object->meshData.getFaceVertices(face)) selection.addVertex(active, vertex);
    }
}

MaterialUse materialUse(const AppContext& ctx, MaterialHandle material) {
    MaterialUse use;
    for (ObjectHandle handle : ctx.scene.objects.handles()) {
        const Object& object = ctx.scene.objects.get(handle);
        if (ctx.scene.materials.resolve(object.material) == material) ++use.objects;
        for (FaceHandle face : object.meshData.getFaceHandles()) {
            if (object.meshData.getFaceMaterial(face) == material && ctx.scene.materials.isValid(material)) ++use.faces;
        }
    }
    return use;
}

std::string describeUse(const MaterialUse& use) {
    if (use.objects == 0 && use.faces == 0) return "unused";

    std::string text;
    if (use.objects > 0) text = std::to_string(use.objects) + (use.objects == 1 ? " object" : " objects");
    if (use.faces > 0) text += (text.empty() ? "" : ", ") + std::to_string(use.faces) + (use.faces == 1 ? " face" : " faces");
    return text;
}

u32 facesWithOwnMaterial(const AppContext& ctx, const Object& object) {
    const MaterialCollection& materials = ctx.scene.materials;
    const MaterialHandle objectMaterial = materials.resolve(object.material);
    u32 count = 0;
    for (FaceHandle face : object.meshData.getFaceHandles()) {
        const MaterialHandle own = object.meshData.getFaceMaterial(face);
        if (materials.isValid(own) && !(own == objectMaterial)) ++count;
    }
    return count;
}

void runMaterialCommand(AppContext& ctx, const CommandArgs& args) {
    if (args.empty()) {
        std::cout << USAGE << std::endl;
    } else if (args[0] == "list") {
        for (MaterialHandle handle : ctx.scene.materials.handles()) printMaterial(ctx, handle);
    } else if (args[0] == "clear" && args.size() == 1) {
        if (!assignsToFaces(ctx)) {
            std::cout << "material: clear works on selected faces in face mode" << std::endl;
            return;
        }
        assignFaceMaterial(ctx, INVALID_MATERIAL);
        std::cout << "[material] the selected faces use their object's material" << std::endl;
    } else if (args[0] == "add") {
        if (args.size() > 2) {
            std::cout << "usage: material add [name]" << std::endl;
            return;
        }
        Material material;
        material.name = ctx.scene.materials.uniqueName(args.size() == 2 ? args[1] : "Material");

        ctx.history.begin(ctx.scene);
        const MaterialHandle handle = ctx.scene.materials.add(material);
        ctx.history.commit();
        printMaterial(ctx, handle);
    } else {
        runMaterialById(ctx, args);
    }
}
