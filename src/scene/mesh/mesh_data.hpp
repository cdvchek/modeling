#pragma once

#include <vector>
#include <unordered_map>
#include <types>

#include "scene/mesh/mesh_types.hpp"
#include "scene/mesh/mesh_array.hpp"
#include "core/math/vec2.hpp"

struct VertexData {
    std::vector<f32> vertices;
    std::vector<u32> indexMap;
};

struct EdgeData {
    std::unordered_map<u32, u32> indexMap;
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

    const std::vector<VertexHandle> getVertexHandles() const;
    const std::vector<EdgeHandle> getEdgeHandles() const;
    const std::vector<FaceHandle> getFaceHandles() const;

    Vec3 getVertexPosition(VertexHandle handle) const;

    std::vector<VertexHandle> getFaceVertices(FaceHandle handle) const;
    // std::vector<u32> getFaceEdges(u32 face) const;
    const std::vector<Triangle>& getFaceTriangles(FaceHandle handle) const;

    void setFacesDirtyByVertex(VertexHandle handle) const;
    void setFacesDirtyByEdge(EdgeHandle handle) const;
    void setFacesDirtyByFace(FaceHandle handlehandle) const;

    // CPU data manipulation

    void positionVertex(VertexHandle handle, Vec3 position);
    void translateVertex(VertexHandle handle, Vec3 delta);
    void scaleVertices(std::vector<VertexHandle> handles, f32 delta);

    //void insertExtrusion(FaceHandle handle);

    // GPU data access
    VertexData getVertexData() const;
    EdgeData getEdgeData(const VertexData& vertexData) const;
    FaceData getFaceData(const VertexData& vertexData) const;

private:
    VertexHandle addVertex(const Vec3& position); // Add a vertex and return its index
    EdgeHandle addEdge(VertexHandle origin, VertexHandle tip); // Add an unpaired half-edge between two vertices
    FaceHandle addFace(const std::vector<VertexHandle>& handles); // Create a face and its half-edge loop from CCW vertices
    FaceHandle addTriangle(VertexHandle v0, VertexHandle v1, VertexHandle v2); // Convenience wrapper for creating a triangle
    FaceHandle addQuad(VertexHandle v0, VertexHandle v1, VertexHandle v2, VertexHandle v3); // Convenience wrapper for creating a quad
    VertexHandle duplicateVertex(VertexHandle handle); // Duplicate a vertex at the same position

    void pairEdges(EdgeHandle a, EdgeHandle b); // Pair two half-edges
    void unpairEdge(EdgeHandle handle); // Disconnect an edge from its pair
    void setEdgeTip(EdgeHandle edge, VertexHandle tip); // Change an edge's tip and dirty the affected face
    void setVertexEdge(VertexHandle vertex, EdgeHandle edge); // Set a representative outgoing half-edge
    void setFaceEdge(FaceHandle face, EdgeHandle edge); // Set a representative boundary edge for a face

    VertexHandle getEdgeOrigin(EdgeHandle handle) const; // Return the starting vertex of the half edge (not the tip)
    std::vector<EdgeHandle> getFaceEdges(FaceHandle handle) const; // Return all half-edges around a face in order
    // std::vector<u32> getFaceVertices(u32 face) const; // Return all vertices around a face in order
    std::vector<EdgeHandle> getVertexEdges(VertexHandle handle) const; // Return all half-edges (incoming and outgoing) incident to a vertex
    std::vector<EdgeHandle> getVertexIncomingEdges(VertexHandle handle) const; // Return all half-edges whose tip is the vertex
    std::vector<EdgeHandle> getVertexOutgoingEdges(VertexHandle handle) const; // Return all half-edges originating at the vertex
    std::vector<FaceHandle> getVertexFaces(VertexHandle handle) const; // Return all faces touching a vertex
    std::vector<FaceHandle> getAdjacentFaces(FaceHandle handle) const; // Return all faces sharing an edge with a face
    std::vector<VertexHandle> getAdjacentVertices(VertexHandle handle) const; // Return all vertices directly connected to a vertex
    
    bool isBoundaryEdge(EdgeHandle handle) const; // Return true for any half-edge that is adjacent to only one face
    bool isBoundaryVertex(VertexHandle handle) const; // Return true for any vertex that has an incident boundary edge
    bool areVerticesConnected(VertexHandle a, VertexHandle b) const; // Return true if given vertices are adjacent
    bool areFacesAdjacent(FaceHandle a, FaceHandle b) const; // Return true if given faces are adjacent
    EdgeHandle findEdge(VertexHandle origin, VertexHandle tip) const; // Finds a directed half-edge between two vertices, returns invalid index if no half-edge found

    VertexHandle splitEdge(EdgeHandle handle, const Vec3& position); // Insert a vertex into an existing edge
    VertexHandle splitEdge(EdgeHandle handle, f32 t); // Insert a vertex somewhere along an edge
    void collapseEdge(EdgeHandle handle); // Collapse an edge and merge its vertices
    FaceHandle splitFace(FaceHandle face, VertexHandle vertexA, VertexHandle vertexB); // Connect two vertices of a face, splitting it into two faces
    void mergeFaces(FaceHandle handle); // Remove the shared edge between two faces and combine them
    void detachVertexFromFace(VertexHandle vertex, FaceHandle face); // Duplicate/separate a vertex so the face can move independently
    void detachFace(FaceHandle handle); // Duplicate the necessary boundary topology so the face is disconnected from its neighbors
    void removeFace(FaceHandle handle); // Remove a face while leaving appropriate boundary topology
    void removeEdge(EdgeHandle handle); // Remove an edge/paair while repairing references
    void removeVertex(VertexHandle handle); // Remove an unused vertex

    std::vector<Triangle> triangulateFace(FaceHandle handle) const;

    MeshArray<Vertex, VertexHandle> m_vertices;
    MeshArray<Edge, EdgeHandle> m_edges;
    MeshArray<Face, FaceHandle> m_faces;
    
    bool m_dirty = true;
};