#pragma once

#include <utility>
#include <vector>
#include "scene/objects/object_collection.hpp"

// An object's origin is its transform: the point its mesh is built around and the axes it turns on.
// These change the origin while the mesh stays where it is in the world, by moving every vertex the opposite way.
// Its children stay where they are too: their relative transforms are worked out again for the moved origin.
// Origins are given as world transforms.

// Every vertex position, in mesh order (getVertexHandles), for setOrigin to start from
std::vector<Vec3> vertexPositions(const MeshData& mesh);
// The mesh's bounding box in its own space; both corners are the origin for an empty mesh
void localBounds(const MeshData& mesh, Vec3& low, Vec3& high);

// What setOrigin starts from: the origin in the world, the mesh, and where each child is in the world
struct OriginStart {
    Transform world;
    std::vector<Vec3> vertices;
    std::vector<std::pair<ObjectHandle, Transform>> children;
};

OriginStart captureOrigin(const ObjectCollection& objects, ObjectHandle handle);

// Moves the origin from start to the world transform to. Working from the start each time keeps a long drag from drifting.
void setOrigin(ObjectCollection& objects, ObjectHandle handle, const OriginStart& start, const Transform& to);

// Same, from where the origin is now
void setOrigin(ObjectCollection& objects, ObjectHandle handle, const Transform& to);

// Origin targets in the world, keeping the origin's rotation and scale unless they say otherwise
// The middle of the mesh's bounding box (in its own axes)
Transform originAtCenter(const ObjectCollection& objects, ObjectHandle handle);
// The middle of the bounding box's bottom (the lowest side along the object's own Y), where an asset stands
Transform originAtBottom(const ObjectCollection& objects, ObjectHandle handle);
// The average of these vertices
Transform originAtVertices(const ObjectCollection& objects, ObjectHandle handle, const std::vector<VertexHandle>& vertices);
// At the world's 0, 0, 0
Transform originAtWorld(const ObjectCollection& objects, ObjectHandle handle);
// Turned back to line up with the world's axes, where it is
Transform originAlignedToWorld(const ObjectCollection& objects, ObjectHandle handle);
