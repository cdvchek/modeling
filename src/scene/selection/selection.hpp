#pragma once

#include <types>
#include <vector>

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

private:
    std::vector<VertexSelection> m_selectedVertices;
};