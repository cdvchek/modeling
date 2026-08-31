#pragma once

#include <types>
#include "scene/mesh/mesh_array.hpp"

class IMesh {
public:
    virtual ~IMesh() = default;
    virtual void drawVertex(VertexHandle handle) const = 0;
    virtual void drawVertices() const = 0;
    virtual void drawEdge(EdgeHandle handle) const = 0;
    virtual void drawEdges() const = 0;
    virtual void drawFace(FaceHandle handle) const = 0;
    virtual void drawFaces() const = 0;
};