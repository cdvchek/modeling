#include "scene/selection/selection.hpp"

#include <algorithm>

void Selection::clear() {
    m_selectedVertices.clear();
    m_selectedEdges.clear();
    m_selectedFaces.clear();
    m_originalPositions.clear();
}

void Selection::addVertex(u32 object, VertexHandle vertex) {
    if (hasVertex(object, vertex)) {
        return;
    }

    m_selectedVertices.push_back({
        object,
        vertex
    });
}

void Selection::removeVertex(u32 object, VertexHandle vertex) {
    auto it = std::remove_if(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [object, vertex](const VertexSelection& selection) {
            return selection.objectIndex == object &&
                   selection.vertex == vertex;
        }
    );

    m_selectedVertices.erase(it, m_selectedVertices.end());
}

void Selection::addEdge(u32 object, EdgeHandle edge) {
    if (hasEdge(object, edge)) {
        return;
    }

    m_selectedEdges.push_back({
        object,
        edge
    });
}

void Selection::removeEdge(u32 object, EdgeHandle edge) {
    auto it = std::remove_if(
        m_selectedEdges.begin(),
        m_selectedEdges.end(),
        [object, edge](const EdgeSelection& selection) {
            return selection.objectIndex == object &&
                   selection.edge == edge;
        }
    );

    m_selectedEdges.erase(it, m_selectedEdges.end());
}

void Selection::addFace(u32 object, FaceHandle face) {
    if (hasFace(object, face)) {
        return;
    }

    m_selectedFaces.push_back({
        object,
        face
    });
}

void Selection::removeFace(u32 object, FaceHandle face) {
    auto it = std::remove_if(
        m_selectedFaces.begin(),
        m_selectedFaces.end(),
        [object, face](const FaceSelection& selection) {
            return selection.objectIndex == object &&
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
    u32 object,
    VertexHandle vertex
) const {
    return std::any_of(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [object, vertex](const VertexSelection& selection) {
            return selection.objectIndex == object &&
                   selection.vertex == vertex;
        }
    );
}

bool Selection::hasEdge(
    u32 object,
    EdgeHandle edge
) const {
    return std::any_of(
        m_selectedEdges.begin(),
        m_selectedEdges.end(),
        [object, edge](const EdgeSelection& selection) {
            return selection.objectIndex == object &&
                   selection.edge == edge;
        }
    );
}

bool Selection::hasFace(
    u32 object,
    FaceHandle face
) const {
    return std::any_of(
        m_selectedFaces.begin(),
        m_selectedFaces.end(),
        [object, face](const FaceSelection& selection) {
            return selection.objectIndex == object &&
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