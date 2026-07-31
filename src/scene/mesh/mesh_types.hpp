#pragma once

#include "core/math/vec3.hpp"

#include <types>
#include <vector>
// #include <limits>

// constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();

struct Vertex;
struct Face;

struct Edge {
    u32 pair = INVALID_INDEX; // parallel edge
    u32 next = INVALID_INDEX; // next edge after in the loop
    u32 prev = INVALID_INDEX; // prev edge before in the loop

    u32 tip = INVALID_INDEX; // vertex that this edge is pointing towards
    u32 face = INVALID_INDEX; // face that is to the left of this edge
};

struct Vertex {
    Vec3 position;
    u32 edge = INVALID_INDEX; // edge that is coming out of this vertex
};

struct Triangle {
    u32 v0;
    u32 v1;
    u32 v2;
};

struct Face {
    u32 edge = INVALID_INDEX; // edge that belongs to the loop that circles this face

    std::vector<Triangle> triangles;
    bool triangulationDirty = true;
};

struct PackagedMesh {
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
};

enum class PresetMesh {
    Cube,
};