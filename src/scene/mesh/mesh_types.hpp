#pragma once

#include "core/math/vec3.hpp"
#include "scene/mesh/mesh_array.hpp"

#include <types>
#include <vector>

struct Vertex;
struct Face;

struct Edge {
    EdgeHandle pair{}; // parallel edge
    EdgeHandle next{}; // next edge after in the loop
    EdgeHandle prev{}; // prev edge before in the loop

    VertexHandle tip{}; // vertex that this edge is pointing towards
    FaceHandle face{}; // face that is to the left of this edge
};

struct Vertex {
    Vec3 position;
    EdgeHandle edge{}; // edge that is coming out of this vertex
};

struct Triangle {
    VertexHandle v0;
    VertexHandle v1;
    VertexHandle v2;
};

struct Face {
    EdgeHandle edge{}; // edge that belongs to the loop that circles this face

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