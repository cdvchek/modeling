#pragma once

#include <types>

#include <vector>

#include "scene/mesh/mesh_handles.hpp"
#include "scene/lights/light.hpp"
#include "scene/references/reference_image.hpp"
#include "scene/objects/object_collection.hpp"
#include "core/math/vec3.hpp"
#include "scene/transform.hpp"

struct VertexSelection {
    ObjectHandle object;
    VertexHandle vertex;
};

struct EdgeSelection {
    ObjectHandle object;
    EdgeHandle edge;
};

struct FaceSelection {
    ObjectHandle object;
    FaceHandle face;
};

class Selection {
public:
    // Clears selected elements, objects, lights, origins, and reference images; the active object stays
    void clear();

    // The one object whose elements can be selected and edited; changing it clears the selected elements
    ObjectHandle getActiveObject() const { return m_activeObject; }
    void setActiveObject(ObjectHandle object);

    // Lights and mesh elements are never selected together; picking one kind clears the other
    void clearMeshElements();
    void clearLights();

    void addVertex(ObjectHandle object, VertexHandle vertex);
    void removeVertex(ObjectHandle object, VertexHandle vertex);

    void addEdge(ObjectHandle object, EdgeHandle edge);
    void removeEdge(ObjectHandle object, EdgeHandle edge);

    void addFace(ObjectHandle object, FaceHandle face);
    void removeFace(ObjectHandle object, FaceHandle face);

    // Whole objects, selected in object mode
    void selectObject(ObjectHandle object);
    void deselectObject(ObjectHandle object);
    bool hasObject(ObjectHandle object) const;
    bool hasObjects() const;
    const std::vector<ObjectHandle>& getObjects() const;
    void clearObjects();

    // An object's origin, picked on its own: selecting it makes that object active and clears everything else,
    // and selecting anything else clears it. Only one origin is selected at a time.
    void selectOrigin(ObjectHandle object);
    void clearOrigin() { m_selectedOrigin = INVALID_OBJECT; }
    bool hasOrigin() const { return !m_selectedOrigin.isNull(); }
    ObjectHandle getOrigin() const { return m_selectedOrigin; }

    void addLight(LightHandle light);
    void removeLight(LightHandle light);
    bool hasLight(LightHandle light) const;
    bool hasLights() const;
    const std::vector<LightHandle>& getLights() const;

    // Reference images are only ever selected on their own: selecting one clears everything else, and selecting
    // anything else clears them
    void addReference(ReferenceHandle reference);
    void removeReference(ReferenceHandle reference);
    bool hasReference(ReferenceHandle reference) const;
    bool hasReferences() const { return !m_selectedReferences.empty(); }
    const std::vector<ReferenceHandle>& getReferences() const { return m_selectedReferences; }
    void clearReferences();

    bool hasVertices() const;
    bool hasEdges() const;
    bool hasFaces() const;

    bool hasVertex(ObjectHandle object, VertexHandle vertex) const;
    bool hasEdge(ObjectHandle object, EdgeHandle edge) const;
    bool hasFace(ObjectHandle object, FaceHandle face) const;

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

    // Light positions saved when a modal tool starts, in the same order as getLights()
    const std::vector<Vec3>& getLightStartPositions() const;
    void setLightStartPositions(const std::vector<Vec3>& positions);
    const std::vector<Vec3>& getLightStartDirections() const;
    void setLightStartDirections(const std::vector<Vec3>& directions);

    // Object transforms saved when a modal tool starts, in the same order as getObjects()
    const std::vector<Transform>& getObjectStartTransforms() const;
    void setObjectStartTransforms(const std::vector<Transform>& transforms);

    // Reference image placements saved when a modal tool starts, in the same order as getReferences()
    // (scale.y holds the size)
    const std::vector<Transform>& getReferenceStartTransforms() const { return m_referenceStartTransforms; }
    void setReferenceStartTransforms(const std::vector<Transform>& transforms) { m_referenceStartTransforms = transforms; }

private:
    std::vector<VertexSelection> m_selectedVertices;
    std::vector<EdgeSelection> m_selectedEdges;
    std::vector<FaceSelection> m_selectedFaces;
    std::vector<LightHandle> m_selectedLights;
    std::vector<ObjectHandle> m_selectedObjects;
    std::vector<ReferenceHandle> m_selectedReferences;
    ObjectHandle m_activeObject = INVALID_OBJECT;
    ObjectHandle m_selectedOrigin = INVALID_OBJECT;

    std::vector<Vec3> m_originalPositions;
    std::vector<Vec3> m_lightStartPositions;
    std::vector<Vec3> m_lightStartDirections;
    std::vector<Transform> m_objectStartTransforms;
    std::vector<Transform> m_referenceStartTransforms;
};