#pragma once

#include "scene/transform.hpp"
#include "renderer/opengl/opengl_mesh.hpp"
#include "scene/mesh_data.hpp"

struct RenderObject {
    Transform transform;
    OpenGLMesh gpuMesh;
    MeshData meshData;
};