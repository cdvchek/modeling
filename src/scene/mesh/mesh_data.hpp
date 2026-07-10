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
};

class MeshData {
public:
    const std::vector<Vertex>& getVertices() const;
    void setVertices(std::vector<Vertex> verts);
    u32 addVertex(const Vertex& vertex);
    void moveVertex(u32 index, const Vec3& newPosition);
    void translateVertex(u32 index, const Vec3& delta);

    const std::vector<Edge>& getEdges() const;
    void setEdges(std::vector<Edge> edges);
    u32 addEdge(const Edge& edge);
    void deleteEdge(u32 edgeIndex);

    const std::vector<Face>& getFaces() const;
    void setFaces(std::vector<Face> faces);
    u32 addFace(const Face& face);
    void deleteFace(u32 faceIndex);

    // const std::vector<u32>& getIndices();

private:
    // void generateIndices();

    std::vector<Vertex> m_vertices;
    std::vector<Edge> m_edges;
    std::vector<Face> m_faces;
    
    bool m_dirty = true;
};