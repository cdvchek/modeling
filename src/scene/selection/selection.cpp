#include "scene/selection/selection.hpp"

#include <algorithm>

void Selection::clear() {
    m_selectedVertices.clear();
    m_selectedEdges.clear();
    m_selectedFaces.clear();
    m_selectedLights.clear();
    m_originalPositions.clear();
    m_lightStartPositions.clear();
    m_lightStartDirections.clear();
    clearObjects();
    clearOrigin();
    clearReferences();
}

void Selection::setActiveObject(ObjectHandle object) {
    if (object == m_activeObject) return;

    m_activeObject = object;
    clearMeshElements();
    if (m_selectedOrigin != object) clearOrigin();
}

void Selection::selectOrigin(ObjectHandle object) {
    setActiveObject(object);
    clearMeshElements();
    clearLights();
    clearObjects();
    clearReferences();
    m_selectedOrigin = object;
}

void Selection::addReference(ReferenceHandle reference) {
    clearMeshElements();
    clearLights();
    clearObjects();
    clearOrigin();
    if (!hasReference(reference)) m_selectedReferences.push_back(reference);
}

void Selection::removeReference(ReferenceHandle reference) {
    std::erase(m_selectedReferences, reference);
}

bool Selection::hasReference(ReferenceHandle reference) const {
    return std::find(m_selectedReferences.begin(), m_selectedReferences.end(), reference) != m_selectedReferences.end();
}

void Selection::clearReferences() {
    m_selectedReferences.clear();
    m_referenceStartTransforms.clear();
}

void Selection::clearMeshElements() {
    m_selectedVertices.clear();
    m_selectedEdges.clear();
    m_selectedFaces.clear();
    m_originalPositions.clear();
}

void Selection::clearLights() {
    m_selectedLights.clear();
    m_lightStartPositions.clear();
    m_lightStartDirections.clear();
}

void Selection::addLight(LightHandle light) {
    clearOrigin();
    clearReferences();
    if (!hasLight(light)) m_selectedLights.push_back(light);
}

void Selection::removeLight(LightHandle light) {
    m_selectedLights.erase(std::remove(m_selectedLights.begin(), m_selectedLights.end(), light), m_selectedLights.end());
}

bool Selection::hasLight(LightHandle light) const {
    return std::find(m_selectedLights.begin(), m_selectedLights.end(), light) != m_selectedLights.end();
}

void Selection::selectObject(ObjectHandle object) {
    clearOrigin();
    clearReferences();
    if (!hasObject(object)) m_selectedObjects.push_back(object);
}

void Selection::deselectObject(ObjectHandle object) {
    std::erase(m_selectedObjects, object);
}

bool Selection::hasObject(ObjectHandle object) const {
    return std::find(m_selectedObjects.begin(), m_selectedObjects.end(), object) != m_selectedObjects.end();
}

bool Selection::hasObjects() const {
    return !m_selectedObjects.empty();
}

const std::vector<ObjectHandle>& Selection::getObjects() const {
    return m_selectedObjects;
}

void Selection::clearObjects() {
    m_selectedObjects.clear();
    m_objectStartTransforms.clear();
}

bool Selection::hasLights() const {
    return !m_selectedLights.empty();
}

const std::vector<LightHandle>& Selection::getLights() const {
    return m_selectedLights;
}

void Selection::addVertex(ObjectHandle object, VertexHandle vertex) {
    clearOrigin();
    clearReferences();
    if (hasVertex(object, vertex)) {
        return;
    }

    m_selectedVertices.push_back({
        object,
        vertex
    });
}

void Selection::removeVertex(ObjectHandle object, VertexHandle vertex) {
    auto it = std::remove_if(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [object, vertex](const VertexSelection& selection) {
            return selection.object == object &&
                   selection.vertex == vertex;
        }
    );

    m_selectedVertices.erase(it, m_selectedVertices.end());
}

void Selection::addEdge(ObjectHandle object, EdgeHandle edge) {
    clearOrigin();
    clearReferences();
    if (hasEdge(object, edge)) {
        return;
    }

    m_selectedEdges.push_back({
        object,
        edge
    });
}

void Selection::removeEdge(ObjectHandle object, EdgeHandle edge) {
    auto it = std::remove_if(
        m_selectedEdges.begin(),
        m_selectedEdges.end(),
        [object, edge](const EdgeSelection& selection) {
            return selection.object == object &&
                   selection.edge == edge;
        }
    );

    m_selectedEdges.erase(it, m_selectedEdges.end());
}

void Selection::addFace(ObjectHandle object, FaceHandle face) {
    clearOrigin();
    clearReferences();
    if (hasFace(object, face)) {
        return;
    }

    m_selectedFaces.push_back({
        object,
        face
    });
}

void Selection::removeFace(ObjectHandle object, FaceHandle face) {
    auto it = std::remove_if(
        m_selectedFaces.begin(),
        m_selectedFaces.end(),
        [object, face](const FaceSelection& selection) {
            return selection.object == object &&
                   selection.face == face;
        }
    );

    m_selectedFaces.erase(it, m_selectedFaces.end());
}

bool Selection::hasVertices() const {
    return !m_selectedVertices.empty();
}

bool Selection::hasEdges() const {
    return !m_selectedEdges.empty();
}

bool Selection::hasFaces() const {
    return !m_selectedFaces.empty();
}

bool Selection::hasVertex(
    ObjectHandle object,
    VertexHandle vertex
) const {
    return std::any_of(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [object, vertex](const VertexSelection& selection) {
            return selection.object == object &&
                   selection.vertex == vertex;
        }
    );
}

bool Selection::hasEdge(
    ObjectHandle object,
    EdgeHandle edge
) const {
    return std::any_of(
        m_selectedEdges.begin(),
        m_selectedEdges.end(),
        [object, edge](const EdgeSelection& selection) {
            return selection.object == object &&
                   selection.edge == edge;
        }
    );
}

bool Selection::hasFace(
    ObjectHandle object,
    FaceHandle face
) const {
    return std::any_of(
        m_selectedFaces.begin(),
        m_selectedFaces.end(),
        [object, face](const FaceSelection& selection) {
            return selection.object == object &&
                   selection.face == face;
        }
    );
}

const std::vector<VertexSelection>&
Selection::getVertices() const {
    return m_selectedVertices;
}

const std::vector<EdgeSelection>&
Selection::getEdges() const {
    return m_selectedEdges;
}

const std::vector<FaceSelection>&
Selection::getFaces() const {
    return m_selectedFaces;
}

std::vector<VertexHandle>
Selection::getVertexHandles() const {
    std::vector<VertexHandle> handles;

    handles.reserve(m_selectedVertices.size());

    for (const VertexSelection& selection :
         m_selectedVertices) {

        handles.push_back(selection.vertex);
    }

    return handles;
}

std::vector<EdgeHandle>
Selection::getEdgeHandles() const {
    std::vector<EdgeHandle> handles;

    handles.reserve(m_selectedEdges.size());

    for (const EdgeSelection& selection :
         m_selectedEdges) {

        handles.push_back(selection.edge);
    }

    return handles;
}

std::vector<FaceHandle>
Selection::getFaceHandles() const {
    std::vector<FaceHandle> handles;

    handles.reserve(m_selectedFaces.size());

    for (const FaceSelection& selection :
         m_selectedFaces) {

        handles.push_back(selection.face);
    }

    return handles;
}

u32 Selection::getNumberOfVertices() const {
    return static_cast<u32>(
        m_selectedVertices.size()
    );
}

u32 Selection::getNumberOfEdges() const {
    return static_cast<u32>(
        m_selectedEdges.size()
    );
}

u32 Selection::getNumberOfFaces() const {
    return static_cast<u32>(
        m_selectedFaces.size()
    );
}

const std::vector<Vec3>&
Selection::getSelectionStartPositions() const {
    return m_originalPositions;
}

void Selection::setSelectionStartPositions(
    const std::vector<Vec3>& positions
) {
    m_originalPositions = positions;
}

const std::vector<Vec3>& Selection::getLightStartPositions() const {
    return m_lightStartPositions;
}

void Selection::setLightStartPositions(const std::vector<Vec3>& positions) {
    m_lightStartPositions = positions;
}

const std::vector<Vec3>& Selection::getLightStartDirections() const {
    return m_lightStartDirections;
}

void Selection::setLightStartDirections(const std::vector<Vec3>& directions) {
    m_lightStartDirections = directions;
}
const std::vector<Transform>& Selection::getObjectStartTransforms() const {
    return m_objectStartTransforms;
}

void Selection::setObjectStartTransforms(const std::vector<Transform>& transforms) {
    m_objectStartTransforms = transforms;
}
