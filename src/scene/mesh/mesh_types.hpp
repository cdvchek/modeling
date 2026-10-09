#pragma once

#include "core/math/vec2.hpp"
#include "core/math/vec3.hpp"
#include "scene/mesh/mesh_handles.hpp"
#include "scene/materials/material.hpp"

#include <types>
#include <vector>

struct Vertex;
struct Face;

// How a mesh's faces meet: each its own flat facet, smoothed together, or smoothed below an angle
enum class ShadingMode : u8 {
    Flat,
    Smooth,
    Auto
};

// An edge's own say in smooth shading, over the mesh's mode; kept the same on both half-edges
enum class EdgeMark : u8 {
    None,
    Hard,
    Smooth
};

struct Edge {
    EdgeHandle pair = INVALID_EDGE;
    EdgeHandle next = INVALID_EDGE;
    EdgeHandle prev = INVALID_EDGE;

    VertexHandle tip = INVALID_VERTEX;
    FaceHandle face = INVALID_FACE; // face that is to the left of this edge

    // Texture coordinates of the face's corner at tip (unused on a border half-edge). (0, 0) is the image's top left,
    // v going down, as glTF has it.
    Vec2 uv;
    EdgeMark mark = EdgeMark::None;
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
    // The face's own material; not valid (none, or removed) means the object's
    MaterialHandle material = INVALID_MATERIAL;

    mutable std::vector<Triangle> triangles;
    mutable bool triangulationDirty = true;
};

struct PackagedMesh {
    DynamicArray<Vertex, VertexHandle> vertices;
    DynamicArray<Edge, EdgeHandle> edges;
    DynamicArray<Face, FaceHandle> faces;
};

enum class PresetMesh {
    Cube,
    Plane,
    Grid,
    Circle,
    Cylinder,
    Cone,
    UVSphere,
    IcoSphere,
    Torus,
};