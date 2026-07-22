#pragma once

#include <types>

class IMesh {
public:
    virtual ~IMesh() = default;
    virtual void drawVertex(u32 selectedVertex) const = 0;
    virtual void drawVertices() const = 0;
    virtual void drawEdges() const = 0;
    virtual void drawFaces() const = 0;
};