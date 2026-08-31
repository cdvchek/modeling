#pragma once

#include <types>

#include <vector>

#include "scene/mesh/mesh_array.hpp"
#include "core/math/vec3.hpp"

struct VertexSelection {
    u32 objectIndex;
    VertexHandle vertex;
};

struct EdgeSelection {
    u32 objectIndex;
    EdgeHandle edge;
};

struct FaceSelection {
    u32 objectIndex;
    FaceHandle face;
};

class Selection {
public:
    void clear();

    void addVertex(u32 object, VertexHandle vertex);
    void removeVertex(u32 object, VertexHandle vertex);

    void addEdge(u32 object, EdgeHandle edge);
    void removeEdge(u32 object, EdgeHandle edge);

    void addFace(u32 object, FaceHandle face);
    void removeFace(u32 object, FaceHandle face);

    bool hasVertices() const;
    bool hasEdges() const;
    bool hasFaces() const;

    bool hasVertex(u32 object, VertexHandle vertex) const;
    bool hasEdge(u32 object, EdgeHandle edge) const;
    bool hasFace(u32 object, FaceHandle face) const;

    const std::vector<VertexSelection>& getVertices() const;
    const std::vector<EdgeSelection>& getEdges() const;
    const std::vector<FaceSelection>& getFaces() const;

    std::vector<VertexHandle> getVertexHandles() const;
    std::vector<EdgeHandle> getEdgeHandles() const;
    std::vector<FaceHandle> getFaceHandles() const;

    u32 getNumberOfVertices() const;
    u32 getNumberOfEdges() const;
    u32 getNumberOfFaces() const;

    const std::vector<Vec3>& getSelectionStartPositions() const;
    void setSelectionStartPositions(const std::vector<Vec3>& positions);

private:
    std::vector<VertexSelection> m_selectedVertices;
    std::vector<EdgeSelection> m_selectedEdges;
    std::vector<FaceSelection> m_selectedFaces;

    std::vector<Vec3> m_originalPositions;
};