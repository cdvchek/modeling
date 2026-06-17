#pragma once

#include "core/assets/mesh_collection.hpp"

// TODO: shouldnt be a mesh collection, it should be an object collection where each object has the transform, name, cpu mesh data, and the gpu mesh and a bool for dirty or not.

struct Assets {
    MeshCollection meshes;
    //ShaderLibrary shaders;
};