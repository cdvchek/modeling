#include "core/assets/mesh_collection.hpp"

MeshHandle MeshCollection::create(const std::string& name, MeshData data) {
    MeshHandle handle = static_cast<MeshHandle>(m_meshes.size());

    m_meshes.push_back(MeshEntry{
        name,
        std::move(data)
    });

    m_nameToHandle[name] = handle;

    return handle;
}

MeshData& MeshCollection::get(MeshHandle handle) {
    return m_meshes[handle].data;
}

const MeshData& MeshCollection::get(MeshHandle handle) const {
    return m_meshes[handle].data;
}

MeshHandle MeshCollection::find(const std::string& name) const {
    auto it = m_nameToHandle.find(name);
    if (it == m_nameToHandle.end()) return INVALID_MESH;

    return it->second;
}