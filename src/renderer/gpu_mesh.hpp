#pragma once

#include <types>
#include <vector>
#include "scene/mesh/mesh_handles.hpp"
#include "scene/mesh/mesh_data.hpp"

class IMesh {
public:
    virtual ~IMesh() = default;
    virtual void drawVertices() const = 0;
    virtual void drawEdges() const = 0;
    virtual void drawFaces() const = 0;

    // The faces' triangles in runs, one per material (see FaceGroup); drawFaceRange draws one run
    virtual const std::vector<FaceGroup>& faceGroups() const = 0;
    virtual void drawFaceRange(u32 firstIndex, u32 indexCount) const = 0;

    // A set of elements (a selection) in one draw call; the set's indices are uploaded again only when it changes
    virtual void drawVertexSet(const std::vector<VertexHandle>& vertices) = 0;
    virtual void drawEdgeSet(const std::vector<EdgeHandle>& edges) = 0;
    virtual void drawFaceSet(const std::vector<FaceHandle>& faces) = 0;
    // A second edge set, kept apart from the selection's so neither uploads every frame
    virtual void drawHardEdgeSet(const std::vector<EdgeHandle>& edges) = 0;
};
