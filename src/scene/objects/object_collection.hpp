#pragma once

#include <string>
#include <vector>
#include "core/containers/dynamic_array.hpp"
#include "scene/transform.hpp"
#include "scene/mesh/mesh_data.hpp"

using ObjectHandle = Handle<struct Object>;

// Scene data only; GPU copies of the mesh live in the application, so objects can be copied freely (undo)
struct Object {
    std::string name;
    Transform transform;        // relative to the parent (the world, for a top-level object)
    MeshData meshData;
    ObjectHandle parent { INVALID_INDEX, 0 };

    bool meshDirty = true;   // set after any change to meshData so the GPU copy is rebuilt
};

constexpr ObjectHandle INVALID_OBJECT { INVALID_INDEX, 0 };

// One row of the object tree: parents before their children, siblings in slot order
struct HierarchyEntry {
    ObjectHandle handle;
    u32 depth;
};

class ObjectCollection {
public:
    ObjectHandle add(const std::string& name, PresetMesh meshType);
    // Takes a whole object as it is (opening a project)
    ObjectHandle add(Object object);
    void remove(ObjectHandle handle);

    bool isValid(ObjectHandle handle) const;

    Object& get(ObjectHandle handle);
    const Object& get(ObjectHandle handle) const;

    Object* tryGet(ObjectHandle handle);
    const Object* tryGet(ObjectHandle handle) const;

    std::vector<ObjectHandle> handles() const;
    ObjectHandle handleAt(u32 slot) const;
    u32 count() const;

    // base, or "base N" with the lowest N that isn't taken
    std::string uniqueName(const std::string& base) const;

    void markAllDirty();

    // ---- Parenting: a child's transform is relative to its parent (see combineTransforms) ----

    ObjectHandle parentOf(ObjectHandle handle) const;
    std::vector<ObjectHandle> childrenOf(ObjectHandle handle) const;
    // ancestor is handle's parent, or its parent's parent, and so on
    bool isAncestor(ObjectHandle ancestor, ObjectHandle handle) const;
    // The object at the top of handle's family (handle itself when it has no parent)
    ObjectHandle topLevelOf(ObjectHandle handle) const;
    std::vector<HierarchyEntry> hierarchy() const;

    // Where the object is in the world: its transform combined with every parent's
    Transform worldTransform(ObjectHandle handle) const;
    Mat4 worldMatrix(ObjectHandle handle) const;
    // The parent's world transform, or the identity for a top-level object
    Transform parentWorldTransform(ObjectHandle handle) const;
    // Puts the object at world by setting its relative transform
    void setWorldTransform(ObjectHandle handle, const Transform& world);
    // Moves its origin to a world position, leaving its relative rotation and scale exactly as they are
    void setWorldPosition(ObjectHandle handle, const Vec3& position);

    // Gives child a new parent (INVALID_OBJECT: none), keeping it where it is in the world.
    // False, changing nothing, if either handle is invalid or the parent is the child or one of its children.
    bool setParent(ObjectHandle child, ObjectHandle parent);

private:
    DynamicArray<Object> m_objects;
};
