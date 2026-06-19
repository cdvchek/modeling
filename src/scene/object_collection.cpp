#include "scene/object_collection.hpp"

Object& ObjectCollection::create(const std::string& name, MeshData meshData) {
    Object object;
    object.name = name;
    object.meshData = std::move(meshData);
    object.meshDirty = true;

    m_objects.push_back(std::move(object));
    return m_objects.back();
}

Object& ObjectCollection::get(u32 index) {
    return m_objects[index];
}

const Object& ObjectCollection::get(u32 index) const {
    return m_objects[index];
}

u32 ObjectCollection::count() const {
    return static_cast<u32>(m_objects.size());
}

std::vector<Object>& ObjectCollection::all() {
    return m_objects;
}

const std::vector<Object>& ObjectCollection::all() const {
    return m_objects;
}