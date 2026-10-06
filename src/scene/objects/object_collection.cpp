#include "scene/objects/object_collection.hpp"

ObjectHandle ObjectCollection::add(const std::string& name, PresetMesh meshType) {
    Object object;
    object.name = name;
    object.meshData.setMesh(meshType);
    object.meshDirty = true;

    return m_objects.insert(object);
}

void ObjectCollection::remove(ObjectHandle handle) {
    m_objects.remove(handle);
}

bool ObjectCollection::isValid(ObjectHandle handle) const {
    return m_objects.isValid(handle);
}

Object& ObjectCollection::get(ObjectHandle handle) {
    return m_objects.get(handle);
}

const Object& ObjectCollection::get(ObjectHandle handle) const {
    return m_objects.get(handle);
}

Object* ObjectCollection::tryGet(ObjectHandle handle) {
    return m_objects.tryGet(handle);
}

const Object* ObjectCollection::tryGet(ObjectHandle handle) const {
    return m_objects.tryGet(handle);
}

std::vector<ObjectHandle> ObjectCollection::handles() const {
    return m_objects.getActiveHandles();
}

ObjectHandle ObjectCollection::handleAt(u32 slot) const {
    if (slot >= m_objects.size()) return INVALID_OBJECT;
    return m_objects.getHandle(slot);
}

u32 ObjectCollection::count() const {
    return m_objects.activeSize();
}

std::string ObjectCollection::uniqueName(const std::string& base) const {
    const auto taken = [this](const std::string& name) {
        for (ObjectHandle handle : handles()) {
            if (m_objects.get(handle).name == name) return true;
        }
        return false;
    };

    if (!taken(base)) return base;

    for (u32 number = 2;; ++number) {
        const std::string name = base + " " + std::to_string(number);
        if (!taken(name)) return name;
    }
}

void ObjectCollection::markAllDirty() {
    for (ObjectHandle handle : handles()) {
        m_objects.get(handle).meshDirty = true;
    }
}
