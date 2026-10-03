#pragma once

#include "core/math/vec3.hpp"
#include "scene/mesh/mesh_array.hpp"

#include <types>
#include <vector>

struct Vertex;
struct Face;

struct Edge {
    EdgeHandle pair = INVALID_EDGE;
    EdgeHandle next = INVALID_EDGE;
    EdgeHandle prev = INVALID_EDGE;

    VertexHandle tip = INVALID_VERTEX;
    FaceHandle face = INVALID_FACE; // face that is to the left of this edge
};

struct Vertex {
    Vec3 position;
    EdgeHandle edge = INVALID_EDGE;
};

struct Triangle {
    VertexHandle v0;
    VertexHandle v1;
    VertexHandle v2;
};

struct Face {
    EdgeHandle edge = INVALID_EDGE;

    mutable std::vector<Triangle> triangles;
    mutable bool triangulationDirty = true;
};

struct PackagedMesh {
    MeshArray<Vertex, VertexHandle> vertices;
    MeshArray<Edge, EdgeHandle> edges;
    MeshArray<Face, FaceHandle> faces;
};

enum class PresetMesh {
    Cube,
};