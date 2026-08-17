#pragma once

#include <types>
#include <vector>
#include "core/math/vec3.hpp"

struct VertexSelection {
    u32 objectIndex;
    u32 vertexIndex;
};

struct EdgeSelection {
    u32 objectIndex;
    u32 edgeIndex;
};

struct FaceSelection {
    u32 objectIndex;
    u32 faceIndex;
};

class Selection {
public:
    void clear();
    void addVertex(u32 object, u32 vertex);
    void removeVertex(u32 object, u32 vertex);

    bool hasVertices() const;
    bool hasVertex(u32 object, u32 vertex) const;
    
    const std::vector<VertexSelection>& getVertices() const;
    const std::vector<u32> getVertexIndices() const;
    // const std::vector<u32> getEdgeIndices() const;
    // const std::vector<u32> getFaceIndices() const;
    const std::vector<u32> getSelectionIndices() const;

    u32 getNumberOfVertices() const;
    u32 getNumberOfEdges() const;
    u32 getNumberOfFaces() const;

    const std::vector<Vec3>& getSelectionStartPositions() const;
    void setSelectionStartPositions(const std::vector<Vec3>& positions);

private:
    std::vector<VertexSelection> m_selectedVertices;
    std::vector<EdgeSelection> m_selectedEdges;
    std::vector<FaceSelection> m_selectedFaces;
    std::vector<Vec3> m_original_positions;
};