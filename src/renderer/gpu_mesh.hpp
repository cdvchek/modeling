#pragma once

class IMesh {
public:
    virtual ~IMesh() = default;
    virtual void drawVertices() const = 0;
    virtual void drawEdges() const = 0;
    virtual void drawFaces() const = 0;
};