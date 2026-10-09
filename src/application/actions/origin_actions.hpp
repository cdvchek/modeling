#pragma once

#include <string>
#include "application/app_context.hpp"
#include "scene/objects/origin.hpp"

// Moving an object's origin without moving its mesh (see scene/objects/origin.hpp)

// Where a one-click command puts the origin
enum class OriginTarget : u8 {
    Geometry,      // middle of the mesh's bounding box
    Bottom,        // middle of its bottom, where an asset stands
    World,         // the world's 0, 0, 0
    WorldRotation, // axes lined up with the world's, origin where it is
    Selection      // the average of the selected vertices (edit mode)
};

// The commands work on the selected origin's object (Selection: the active object with vertices selected)
bool canMoveOrigin(const AppContext& ctx, OriginTarget target);
// One undo step; false (and a console error) if there's nothing to move it for
bool moveOrigin(AppContext& ctx, OriginTarget target);

// The object a console `origin` command acts on: the selected origin's, else the active object
ObjectHandle originCommandObject(const AppContext& ctx);

// View menu: shows or hides every origin marker; hiding drops a selected origin
void toggleOrigins(AppContext& ctx);

// Grab and rotate on a selected origin: remember where it, its mesh, and its children started, then each frame set
// the new world transform from those (the mesh and children stay put); cancel is the undo snapshot
void beginOriginEdit(AppContext& ctx);
void updateOriginEdit(AppContext& ctx, const Transform& to);
