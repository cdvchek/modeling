#include "scene/objects/object_collection.hpp"

ObjectHandle ObjectCollection::add(const std::string& name, PresetMesh meshType) {
    Object object;
    object.name = name;
    object.meshData.setMesh(meshType);
    object.meshDirty = true;

    return m_objects.insert(object);
}

ObjectHandle ObjectCollection::add(Object object) {
    object.meshDirty = true;
    return m_objects.insert(object);
}

void ObjectCollection::remove(ObjectHandle handle) {
    if (!m_objects.isValid(handle)) return;

    // Children move up a level (to the grandparent, or the top), staying where they are
    const ObjectHandle grandparent = parentOf(handle);
    for (ObjectHandle child : childrenOf(handle)) setParent(child, grandparent);

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

ObjectHandle ObjectCollection::parentOf(ObjectHandle handle) const {
    const Object* object = m_objects.tryGet(handle);
    return object && m_objects.isValid(object->parent) ? object->parent : INVALID_OBJECT;
}

std::vector<ObjectHandle> ObjectCollection::childrenOf(ObjectHandle handle) const {
    std::vector<ObjectHandle> children;
    for (ObjectHandle other : handles()) {
        if (parentOf(other) == handle) children.push_back(other);
    }
    return children;
}

bool ObjectCollection::isAncestor(ObjectHandle ancestor, ObjectHandle handle) const {
    for (ObjectHandle up = parentOf(handle); !up.isNull(); up = parentOf(up)) {
        if (up == ancestor) return true;
    }
    return false;
}

ObjectHandle ObjectCollection::topLevelOf(ObjectHandle handle) const {
    ObjectHandle top = handle;
    for (ObjectHandle up = parentOf(handle); !up.isNull(); up = parentOf(up)) top = up;
    return top;
}

std::vector<HierarchyEntry> ObjectCollection::hierarchy() const {
    std::vector<HierarchyEntry> rows;
    std::vector<HierarchyEntry> stack;

    // Depth first; siblings pushed in reverse so they come out in slot order
    const std::vector<ObjectHandle> all = handles();
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
        if (parentOf(*it).isNull()) stack.push_back({ *it, 0 });
    }

    while (!stack.empty()) {
        const HierarchyEntry entry = stack.back();
        stack.pop_back();
        rows.push_back(entry);

        const std::vector<ObjectHandle> children = childrenOf(entry.handle);
        for (auto it = children.rbegin(); it != children.rend(); ++it) stack.push_back({ *it, entry.depth + 1 });
    }

    return rows;
}

Transform ObjectCollection::worldTransform(ObjectHandle handle) const {
    const Object* object = m_objects.tryGet(handle);
    if (!object) return Transform();

    // A top-level object's transform is already in the world; returned as is, so its angles read back unchanged
    const ObjectHandle parent = parentOf(handle);
    if (parent.isNull()) return object->transform;
    return combineTransforms(worldTransform(parent), object->transform);
}

Mat4 ObjectCollection::worldMatrix(ObjectHandle handle) const {
    return worldTransform(handle).getMatrix();
}

Transform ObjectCollection::parentWorldTransform(ObjectHandle handle) const {
    const ObjectHandle parent = parentOf(handle);
    return parent.isNull() ? Transform() : worldTransform(parent);
}

void ObjectCollection::setWorldTransform(ObjectHandle handle, const Transform& world) {
    Object* object = m_objects.tryGet(handle);
    if (!object) return;

    const ObjectHandle parent = parentOf(handle);
    object->transform = parent.isNull() ? world : relativeTransform(worldTransform(parent), world);
}

void ObjectCollection::setWorldPosition(ObjectHandle handle, const Vec3& position) {
    Object* object = m_objects.tryGet(handle);
    if (!object) return;

    Transform world = worldTransform(handle);
    world.position = position;
    const Transform parent = parentWorldTransform(handle);
    object->transform.position = parentOf(handle).isNull() ? position : relativeTransform(parent, world).position;
}

bool ObjectCollection::setParent(ObjectHandle child, ObjectHandle parent) {
    if (!m_objects.isValid(child)) return false;
    if (!parent.isNull() && (!m_objects.isValid(parent) || parent == child || isAncestor(child, parent))) return false;

    const Transform world = worldTransform(child);
    m_objects.get(child).parent = parent;
    setWorldTransform(child, world);
    return true;
}

void ObjectCollection::markAllDirty() {
    // Restored copies may hold other positions under the same layout, so every vertex counts as moved
    for (ObjectHandle handle : handles()) {
        Object& object = m_objects.get(handle);
        object.meshDirty = true;
        object.meshData.markAllMoved();
    }
}
