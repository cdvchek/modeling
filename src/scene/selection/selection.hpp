#pragma once

#include <types>
#include <vector>
#include "core/math/vec3.hpp"

struct VertexSelection {
    u32 objectIndex;
    u32 vertexIndex;
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

    const std::vector<Vec3>& getSelectionStartPositions() const;
    void setSelectionStartPositions(const std::vector<Vec3>& positions);

private:
    std::vector<VertexSelection> m_selectedVertices;
    std::vector<Vec3> m_original_positions;
};