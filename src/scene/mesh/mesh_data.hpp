#pragma once

#include <vector>
#include <unordered_map>
#include <types>

#include "scene/mesh/mesh_types.hpp"
#include "scene/mesh/mesh_array.hpp"
#include "core/math/vec2.hpp"

using HalfEdgeIndex = u32;
using RendererIndex = u32;

struct EdgeData {
    std::unordered_map<HalfEdgeIndex, RendererIndex> indexMap;
    std::vector<u32> indices;
};

struct FaceData {
    std::vector<u32> indexMap;
    std::vector<u32> indices;
};

class MeshData {
public:
    void setMesh(PresetMesh meshType);
    const std::vector<Vertex> getVertices() const;
    const std::vector<Face> getFaces() const;

    Vec3 getVertexPosition(u32 vertexIndex) const;

    std::vector<u32> getFaceVertices(u32 face) const;
    // std::vector<u32> getFaceEdges(u32 face) const;
    const std::vector<Triangle>& getFaceTriangles(u32 faceIndex) const;

    void setFacesDirtyByVertex(u32 vertexIndex) const;
    void setFacesDirtyByEdge(u32 edgeIndex) const;
    void setFacesDirtyByFace(u32 faceIndex) const;

    // CPU data manipulation

    void positionVertex(u32 v_index, Vec3 position);
    void positionEdge(u32 e_index, Vec3 position);
    void positionFace(u32 f_index, Vec3 position);

    void translateVertex(u32 v_index, Vec3 delta);
    void translateEdge(u32 e_index, Vec3 delta);
    void translateFace(u32 f_index, Vec3 delta);

    void insertExtrusion(u32 f_index);

    void scaleVertices(std::vector<u32> v_indices, f32 delta);

    // GPU data access
    std::vector<f32> getVertexData() const;
    EdgeData getEdgeData() const;
    FaceData getFaceData() const;

private:
    u32 addVertex(const Vec3& position); // Add a vertex and return its index
    u32 addEdge(u32 origin, u32 tip); // Add an unpaired half-edge between two vertices
    u32 addFace(const std::vector<u32>& vertices); // Create a face and its half-edge loop from CCW vertices
    u32 addTriangle(u32 v0, u32 v1, u32 v2); // Convenience wrapper for creating a triangle
    u32 addQuad(u32 v0, u32 v1, u32 v2, u32 v3); // Convenience wrapper for creating a quad
    u32 duplicateVertex(u32 vertex); // Duplicate a vertex at the same position

    void pairEdges(u32 a, u32 b); // Pair two half-edges
    void unpairEdge(u32 edge); // Disconnect an edge from its pair
    void setEdgeTip(u32 edge, u32 tip); // Change an edge's tip and dirty the affected face
    void setVertexEdge(u32 vertex, u32 edge); // Set a representative outgoing half-edge
    void setFaceEdge(u32 face, u32 edge); // Set a representative boundary edge for a face

    u32 getEdgeOrigin(u32 edge) const; // Return the starting vertex of the half edge (not the tip)
    std::vector<u32> getFaceEdges(u32 face) const; // Return all half-edges around a face in order
    // std::vector<u32> getFaceVertices(u32 face) const; // Return all vertices around a face in order
    std::vector<u32> getVertexEdges(u32 vertex) const; // Return all half-edges (incoming and outgoing) incident to a vertex
    std::vector<u32> getVertexIncomingEdges(u32 vertex) const; // Return all half-edges whose tip is the vertex
    std::vector<u32> getVertexOutgoingEdges(u32 vertex) const; // Return all half-edges originating at the vertex
    std::vector<u32> getVertexFaces(u32 vertex) const; // Return all faces touching a vertex
    std::vector<u32> getAdjacentFaces(u32 face) const; // Return all faces sharing an edge with a face
    std::vector<u32> getAdjacentVertices(u32 vertex) const; // Return all vertices directly connected to a vertex
    
    bool isBoundaryEdge(u32 edge) const; // Return true for any half-edge that is adjacent to only one face
    bool isBoundaryVertex(u32 vertex) const; // Return true for any vertex that has an incident boundary edge
    bool areVerticesConnected(u32 a, u32 b) const; // Return true if given vertices are adjacent
    bool areFacesAdjacent(u32 a, u32 b) const; // Return true if given faces are adjacent
    u32 findEdge(u32 origin, u32 tip) const; // Finds a directed half-edge between two vertices, returns invalid index if no half-edge found

    u32 splitEdge(u32 edge, const Vec3& position); // Insert a vertex into an existing edge
    u32 splitEdge(u32 edge, f32 t); // Insert a vertex somewhere along an edge
    void collapseEdge(u32 edge); // Collapse an edge and merge its vertices
    u32 splitFace(u32 face, u32 vertexA, u32 vertexB); // Connect two vertices of a face, splitting it into two faces
    void mergeFaces(u32 edge); // Remove the shared edge between two faces and combine them
    void detachVertexFromFace(u32 vertex, u32 face); // Duplicate/separate a vertex so the face can move independently
    void detachFace(u32 face); // Duplicate the necessary boundary topology so the face is disconnected from its neighbors
    void removeFace(u32 face); // Remove a face while leaving appropriate boundary topology
    void removeEdge(u32 edge); // Remove an edge/paair while repairing references
    void removeVertex(u32 vertex); // Remove an unused vertex

    std::vector<Triangle> triangulateFace(u32 faceIndex) const;

    MeshArray<Vertex, VertexHandle> m_vertices;
    MeshArray<Edge, EdgeHandle> m_edges;
    MeshArray<Face, FaceHandle> m_faces;
    
    bool m_dirty = true;
};