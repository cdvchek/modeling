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
    void addVertex(u32 object, u32 vertex, Vec3 position);
    void removeVertex(u32 object, u32 vertex);

    bool hasVertices() const;
    bool hasVertex(u32 object, u32 vertex) const;
    
    const std::vector<VertexSelection>& getVertices() const;
    const std::vector<u32> getVertexIndices() const;

    Vec3 getSelectionDelta(Vec3 currentPos) const;
    void setSelectionStartPosition(Vec3 position);

private:
    std::vector<VertexSelection> m_selectedVertices;

    Vec3 m_original_position = Vec3();
};