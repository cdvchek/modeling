#pragma once

#include "core/math/vec3.hpp"

#include <types>
#include <vector>
#include <limits>

constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();

struct Vertex;
struct Face;

struct Edge {
    u32 pair = INVALID_INDEX;
    u32 next = INVALID_INDEX;
    u32 prev = INVALID_INDEX;

    u32 tip = INVALID_INDEX;
    u32 face = INVALID_INDEX;
};

struct Vertex {
    Vec3 position;
    u32 edge = INVALID_INDEX;
};

struct Face {
    u32 edge = INVALID_INDEX;
};

struct PackagedMesh {
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
};

enum class PresetMesh {
    Cube,
};