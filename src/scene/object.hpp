#pragma once

#include <string>
#include "scene/transform.hpp"
#include "scene/mesh_data.hpp"
#include "renderer/opengl/opengl_mesh.hpp"

struct Object {
    std::string name;
    Transform transform;
    MeshData meshData;
    OpenGLMesh gpuMesh;

    bool meshDirty = true;
};