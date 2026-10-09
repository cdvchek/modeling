#pragma once

#include "application/app_context.hpp"

// material list | add [name] | clear | <id> [remove | assign [<object id> ...] | select | <property> <value>]
void runMaterialCommand(AppContext& ctx, const CommandArgs& args);

// Face mode with faces selected: assigning goes to those faces rather than to objects
bool assignsToFaces(const AppContext& ctx);

// The objects "assign" acts on without ids: the selected objects in object mode, otherwise the active object
std::vector<ObjectHandle> materialTargets(const AppContext& ctx);

// Gives each object the material, as one undo step
void assignMaterial(AppContext& ctx, const std::vector<ObjectHandle>& objects, MaterialHandle material);

// Gives the active object's selected faces the material, or takes their own away (INVALID_MATERIAL) so they use
// the object's again; one undo step
void assignFaceMaterial(AppContext& ctx, MaterialHandle material);

// Face mode: selects the active object's faces that show the material (their own, or the object's when they have none)
void selectFacesWithMaterial(AppContext& ctx, MaterialHandle material);

// Removes a material (never Default) as one undo step; its objects go back to Default, its faces to their object's
bool removeMaterial(AppContext& ctx, MaterialHandle material);

// How many objects use the material as theirs (Default counts the objects with none), and how many faces have it
// as their own
struct MaterialUse {
    u32 objects = 0;
    u32 faces = 0;
};
MaterialUse materialUse(const AppContext& ctx, MaterialHandle material);
// "2 objects, 40 faces", "1 object", "unused"
std::string describeUse(const MaterialUse& use);

// How many of the object's faces have a material of their own that differs from the object's
u32 facesWithOwnMaterial(const AppContext& ctx, const Object& object);
