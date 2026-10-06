#include "application/object_meshes.hpp"

OpenGLMesh* ObjectMeshCache::sync(ObjectHandle handle, Object& object) {
    if (handle.index >= m_entries.size()) m_entries.resize(handle.index + 1);
    Entry& entry = m_entries[handle.index];

    // A different object now lives in this slot: start fresh
    if (entry.used && entry.generation != handle.generation) {
        entry.mesh.destroy();
        entry.used = false;
    }

    if (!entry.used) {
        entry.mesh.create(object.meshData);
        entry.used = true;
        entry.generation = handle.generation;
        object.meshDirty = false;
    } else if (object.meshDirty) {
        entry.mesh.update(object.meshData);
        object.meshDirty = false;
    }

    return &entry.mesh;
}

void ObjectMeshCache::prune(const ObjectCollection& objects) {
    for (u32 slot = 0; slot < m_entries.size(); ++slot) {
        Entry& entry = m_entries[slot];
        if (entry.used && !objects.isValid({ slot, entry.generation })) {
            entry.mesh.destroy();
            entry.used = false;
        }
    }
}
