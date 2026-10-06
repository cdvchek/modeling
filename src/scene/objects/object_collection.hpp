#pragma once

#include <string>
#include <vector>
#include "core/containers/dynamic_array.hpp"
#include "scene/transform.hpp"
#include "scene/mesh/mesh_data.hpp"

// Scene data only; GPU copies of the mesh live in the application, so objects can be copied freely (undo)
struct Object {
    std::string name;
    Transform transform;
    MeshData meshData;

    bool meshDirty = true;   // set after any change to meshData so the GPU copy is rebuilt
};

using ObjectHandle = Handle<Object>;

constexpr ObjectHandle INVALID_OBJECT { INVALID_INDEX, 0 };

class ObjectCollection {
public:
    ObjectHandle add(const std::string& name, PresetMesh meshType);
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

private:
    DynamicArray<Object> m_objects;
};
