#pragma once

#include <types>

class IMesh {
public:
    virtual ~IMesh() = default;
    virtual void drawVertex(u32 index) const = 0;
    virtual void drawVertices() const = 0;
    virtual void drawEdge(u32 index) const = 0;
    virtual void drawEdges() const = 0;
    virtual void drawFace(u32 index) const = 0;
    virtual void drawFaces() const = 0;
};