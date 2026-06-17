#pragma once

#include "core/scene/transform.hpp"
#include "renderer/opengl/opengl_mesh.hpp"
#include "core/assets/meshes/mesh.hpp"

struct RenderObject {
    Transform transform;
    OpenGLMesh gpuMesh;
    MeshData meshData;
};