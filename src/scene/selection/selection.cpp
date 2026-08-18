#include "scene/selection/selection.hpp"

#include <algorithm>

void Selection::clear() {
    m_selectedVertices.clear();
    m_selectedEdges.clear();
    m_selectedFaces.clear();
}

void Selection::addVertex(u32 object, u32 vertex) {
    auto it = std::find_if(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [object, vertex](const VertexSelection& selection) {
            return selection.objectIndex == object &&
                   selection.vertexIndex == vertex;
        }
    );

    if (it != m_selectedVertices.end()) {
        return;
    }

    m_selectedVertices.push_back({
        object,
        vertex
    });
}

void Selection::removeVertex(u32 object, u32 vertex) {
    auto it = std::remove_if(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [object, vertex](const VertexSelection& selection) {
            return selection.objectIndex == object &&
                   selection.vertexIndex == vertex;
        }
    );

    m_selectedVertices.erase(it, m_selectedVertices.end());
}

void Selection::addFace(u32 object, u32 face) {
    auto it = std::find_if(
        m_selectedFaces.begin(),
        m_selectedFaces.end(),
        [object, face](const FaceSelection& selection) {
            return selection.objectIndex == object &&
                   selection.faceIndex == face;
        }
    );

    if (it != m_selectedFaces.end()) {
        return;
    }

    m_selectedFaces.push_back({
        object,
        face
    });
}

void Selection::removeFace(u32 object, u32 face) {
    auto it = std::remove_if(
        m_selectedFaces.begin(),
        m_selectedFaces.end(),
        [object, face](const FaceSelection& selection) {
            return selection.objectIndex == object &&
                   selection.faceIndex == face;
        }
    );

    m_selectedFaces.erase(it, m_selectedFaces.end());
}

bool Selection::hasVertices() const {
    return !m_selectedVertices.empty();
}

const std::vector<VertexSelection>& Selection::getVertices() const {
    return m_selectedVertices;
}

const std::vector<u32> Selection::getVertexIndices() const {
    std::vector<u32> data(m_selectedVertices.size());
    for (std::size_t i = 0; i < m_selectedVertices.size(); ++i) {
        data[i] = m_selectedVertices[i].vertexIndex;
    }
    return data;
}

// const std::vector<u32> Selection::getEdgeIndices() const {

// }

const std::vector<u32> Selection::getSelectionIndices() const {
    std::vector<u32> data;
    data.resize(m_selectedVertices.size() + m_selectedEdges.size() + m_selectedFaces.size());
    
    for (std::size_t i = 0; i < m_selectedVertices.size(); ++i) {
        data[i] = m_selectedVertices[i].vertexIndex;
    }

    std::size_t edgeOffset = m_selectedVertices.size();
    for (std::size_t i = 0; i < m_selectedEdges.size(); ++i) {
        data[i + edgeOffset] = m_selectedEdges[i].edgeIndex;
    }

    std::size_t faceOffset = edgeOffset + m_selectedEdges.size();
    for (std::size_t i = 0; i < m_selectedFaces.size(); ++i) {
        data[i + faceOffset] = m_selectedFaces[i].faceIndex;
    }

    return data;
}

u32 Selection::getNumberOfVertices() const {
    return static_cast<u32>(m_selectedVertices.size());
}

u32 Selection::getNumberOfEdges() const {
    return static_cast<u32>(m_selectedEdges.size());
}

u32 Selection::getNumberOfFaces() const {
    return static_cast<u32>(m_selectedFaces.size());
}

bool Selection::hasVertex(u32 object, u32 vertex) const {
    return std::any_of(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [object, vertex](const VertexSelection& selection) {
            return selection.objectIndex == object &&
                   selection.vertexIndex == vertex;
        }
    );
}

const std::vector<Vec3>& Selection::getSelectionStartPositions() const {
    return m_original_positions;
}

void Selection::setSelectionStartPositions(const std::vector<Vec3>& positions) {
    m_original_positions = positions;
}