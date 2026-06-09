#pragma once

#include "core/scene/transform.hpp"
#include "renderer/opengl/opengl_mesh.hpp"
#include "core/geometry/mesh.hpp"

struct RenderObject {
    Transform transform;
    OpenGLMesh gpuMesh;
    MeshData meshData;
};