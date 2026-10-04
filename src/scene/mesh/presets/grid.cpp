#include "scene/mesh/mesh_factory.hpp"

PackagedMesh MeshFactory::grid(f32 size, u32 divisions) {
    return plane(size, divisions);
}
