#pragma once

#include <vector>
#include <types>

#include "core/math/vec3.hpp"

struct Vertex {
    Vec3 position;
};

struct Edge {
    u32 v0;
    u32 v1;
};

struct Face {
    std::vector<u32> vertices;
    std::vector<u32> edges;
};

class MeshData {
public:
    std::vector<Vertex> getVertices();
    const std::vector<Vertex> getVertices() const;

    std::vector<u32> getIndices();
    const std::vector<u32> getIndices() const;

    std::vector<Edge> getEdges();
    const std::vector<Edge> getEdges() const;

    std::vector<Face> getFaces();
    const std::vector<Face> getFaces() const;

    // TODO: setters for verts/indices/edges/faces
    // TODO: have this be more of an interface and the indices get generated from the faces

private:
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::vector<Edge> edges;
    std::vector<Face> faces;

    bool dirty = true;
};