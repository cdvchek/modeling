#include "application/viewport/object_meshes.hpp"

OpenGLMesh* ObjectMeshCache::sync(ObjectHandle handle, Object& object, const FaceGroupOf& groupOf, u64 materialsStamp) {
    if (handle.index >= m_entries.size()) m_entries.resize(handle.index + 1);
    Entry& entry = m_entries[handle.index];

    // A different object now lives in this slot: start fresh
    if (entry.used && entry.generation != handle.generation) {
        entry.mesh.destroy();
        entry.used = false;
    }

    if (!entry.used) {
        entry.mesh.create(object.meshData, groupOf, materialsStamp);
        entry.used = true;
        entry.generation = handle.generation;
        object.meshDirty = false;
        object.meshData.clearMoved();
    } else if (object.meshDirty || !entry.mesh.matches(object.meshData, materialsStamp)) {
        // A changed stamp rebuilds even when nothing set meshDirty (shading or UVs changed from the panel, say)
        if (!entry.mesh.patch(object.meshData, materialsStamp)) entry.mesh.update(object.meshData, groupOf, materialsStamp);
        object.meshDirty = false;
        object.meshData.clearMoved();
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
