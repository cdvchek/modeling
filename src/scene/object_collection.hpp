#pragma once

#include <string>
#include <vector>
#include "scene/transform.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "renderer/opengl/opengl_mesh.hpp"

struct Object {
    std::string name;
    Transform transform;
    MeshData meshData;
    OpenGLMesh gpuMesh;

    bool meshDirty = true;
};

class ObjectCollection {
public:
    Object& create(const std::string& name, MeshData mesh);
    Object& get(u32 index);
    const Object& get(u32 index) const;
    u32 count() const;
    std::vector<Object>& all();
    const std::vector<Object>& all() const;
private:
    std::vector<Object> m_objects;
};