#pragma once

#include "core/assets/meshes/mesh.hpp"
#include <vector>
#include <types>
#include <string>
#include <unordered_map>

using MeshHandle = u32;

constexpr MeshHandle INVALID_MESH = UINT32_MAX;

class MeshCollection {
public:
    MeshHandle create(const std::string& name, MeshData data);
    MeshData& get(MeshHandle handle);
    const MeshData& get(MeshHandle handle) const;
    MeshHandle find(const std::string& name) const;
    
private:
    struct MeshEntry {
        std::string name;
        MeshData data;
    };

    std::vector<MeshEntry> m_meshes;
    std::unordered_map<std::string, MeshHandle> m_nameToHandle;
};