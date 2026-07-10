#include "scene/selection/selection.hpp"

#include <algorithm>

void Selection::clear() {
    m_selectedVertices.clear();
}

void Selection::addVertex(u32 object, u32 vertex, Vec3 position) {
    if (m_selectedVertices.empty()) m_original_position = position;

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

bool Selection::hasVertices() const {
    return !m_selectedVertices.empty();
}

const std::vector<VertexSelection>& Selection::getVertices() const {
    return m_selectedVertices;
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

Vec3 Selection::getSelectionDelta(Vec3 currentPos) const {
    return currentPos - m_original_position;
}

void Selection::setSelectionStartPosition(Vec3 position) {
    m_original_position = position;
}