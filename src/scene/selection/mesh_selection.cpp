#include "scene/selection/mesh_selection.hpp"

#include <algorithm>

namespace {
    // Deselect the vertices that no remaining selected edge or face uses
    void deselectUnusedVertices(Selection& selection, ObjectHandle object, const MeshData& mesh, const std::vector<VertexHandle>& vertices) {
        for (VertexHandle vertex : vertices) {
            bool used = false;

            for (EdgeHandle edge : selection.getEdgeHandles()) {
                if (mesh.getEdgeOrigin(edge) == vertex || mesh.getEdgeTip(edge) == vertex) used = true;
            }

            for (FaceHandle face : selection.getFaceHandles()) {
                const std::vector<VertexHandle> corners = mesh.getFaceVertices(face);
                if (std::find(corners.begin(), corners.end(), vertex) != corners.end()) used = true;
            }

            if (!used) selection.removeVertex(object, vertex);
        }
    }
}

bool isEdgeSelected(const Selection& selection, ObjectHandle object, const MeshData& mesh, EdgeHandle edge) {
    return selection.hasEdge(object, edge) || selection.hasEdge(object, mesh.getEdge(edge)->pair);
}

void selectEdge(Selection& selection, ObjectHandle object, const MeshData& mesh, EdgeHandle edge) {
    if (isEdgeSelected(selection, object, mesh, edge)) return;

    selection.addEdge(object, edge);
    selection.addVertex(object, mesh.getEdgeOrigin(edge));
    selection.addVertex(object, mesh.getEdgeTip(edge));
}

void selectFace(Selection& selection, ObjectHandle object, const MeshData& mesh, FaceHandle face) {
    selection.addFace(object, face);

    for (VertexHandle vertex : mesh.getFaceVertices(face)) {
        selection.addVertex(object, vertex);
    }
}

void deselectEdge(Selection& selection, ObjectHandle object, const MeshData& mesh, EdgeHandle edge) {
    selection.removeEdge(object, edge);
    selection.removeEdge(object, mesh.getEdge(edge)->pair);
    deselectUnusedVertices(selection, object, mesh, { mesh.getEdgeOrigin(edge), mesh.getEdgeTip(edge) });
}

void deselectFace(Selection& selection, ObjectHandle object, const MeshData& mesh, FaceHandle face) {
    selection.removeFace(object, face);
    deselectUnusedVertices(selection, object, mesh, mesh.getFaceVertices(face));
}

void selectAll(Selection& selection, ObjectHandle object, const MeshData& mesh, SelectAllMode mode) {
    switch (mode) {
        case SelectAllMode::Vertices:
            for (VertexHandle vertex : mesh.getVertexHandles()) selection.addVertex(object, vertex);
            break;
        case SelectAllMode::Edges:
            for (EdgeHandle edge : mesh.getEdgeHandles()) selectEdge(selection, object, mesh, edge);
            break;
        case SelectAllMode::Faces:
            for (FaceHandle face : mesh.getFaceHandles()) {
                if (!selection.hasFace(object, face)) selectFace(selection, object, mesh, face);
            }
            break;
    }
}
