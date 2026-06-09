#pragma once

#include <vector>
#include <types>

#include "core/math/vec3.hpp"

struct Vertex {
    Vec3 position;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    bool dirty = true;
};