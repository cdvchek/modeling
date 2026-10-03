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
    // ---- Access (mesh_data_access.cpp) ----
    // Look up a single element by handle.

    void setMesh(PresetMesh meshType);

    const Vertex* getVertex(VertexHandle handle) const;
    const Edge* getEdge(EdgeHandle handle) const;
    const Face* getFace(FaceHandle handle) const;

    const std::vector<Vertex> getVertices() const;
    const std::vector<Face> getFaces() const;

    const std::vector<VertexHandle> getVertexHandles() const;
    const std::vector<EdgeHandle> getEdgeHandles() const;
    const std::vector<FaceHandle> getFaceHandles() const;

    VertexHandle getEdgeOrigin(EdgeHandle handle) const;
    VertexHandle getEdgeTip(EdgeHandle handle) const;

    bool isValidHandle(VertexHandle handle) const;
    bool isValidHandle(EdgeHandle handle) const;
    bool isValidHandle(FaceHandle handle) const;

    // ---- Topology queries (mesh_data_queries.cpp) ----
    // Read connectivity. Never modify the mesh.

    std::vector<VertexHandle> getFaceVertices(FaceHandle handle) const;

    std::vector<EdgeHandle> getOutgoingEdges(VertexHandle handle) const;
    std::vector<EdgeHandle> getIncomingEdges(VertexHandle handle) const;
    std::vector<EdgeHandle> getLoopEdges(EdgeHandle start) const;

    bool isBorder(EdgeHandle handle) const;
    bool isBorderVertex(VertexHandle handle) const;

    // Checks every half-edge invariant and prints the first violation.
    // Every half-edge has a pair; border half-edges have no face.
    bool validate() const;

    // ---- Geometry (mesh_data_geometry.cpp) ----
    // Positions and anything derived from them. Never change connectivity.

    Vec3 getVertexPosition(VertexHandle handle) const;
    Vec3 getFaceNormal(FaceHandle handle) const;
    const std::vector<Triangle>& getFaceTriangles(FaceHandle handle) const;

    void positionVertex(VertexHandle handle, Vec3 position);
    void translateVertex(VertexHandle handle, Vec3 delta);

    void setFacesDirtyByVertex(VertexHandle handle) const;
    void setFacesDirtyByEdge(EdgeHandle handle) const;
    void setFacesDirtyByFace(FaceHandle handle) const;

    // ---- Operators (mesh_data_ops.cpp, mesh_data_merge.cpp, mesh_data_dissolve.cpp) ----
    // Complete edits. The mesh must be valid before and after each call.

    FaceHandle insertFaceRing(FaceHandle handle);
    VertexHandle splitEdge(EdgeHandle handle);

    bool removeVertex(VertexHandle handle);
    bool removeEdge(EdgeHandle handle);
    bool removeFace(FaceHandle handle);

    bool fillFaceLoop(EdgeHandle handle);
    bool connectVertices(VertexHandle a, VertexHandle b);
    bool mergeVertices(VertexHandle a, VertexHandle b, u8 mergeType);

    VertexHandle dissolveEdge(EdgeHandle handle);
    VertexHandle dissolveFace(FaceHandle handle);


    // ---- GPU export (mesh_data_gpu.cpp) ----
    // Flatten the mesh into buffers for rendering.

    VertexData getVertexData() const;
    EdgeData getEdgeData(const VertexData& vertexData) const;
    FaceData getFaceData(const VertexData& vertexData) const;

private:
    // ---- Topology queries (mesh_data_queries.cpp) ----

    std::vector<EdgeHandle> getFaceEdges(FaceHandle handle) const;
    std::vector<VertexHandle> getVertexNeighbors(VertexHandle handle) const;
    EdgeHandle findEdge(VertexHandle origin, VertexHandle tip) const;

    // ---- Geometry (mesh_data_geometry.cpp) ----

    std::vector<Triangle> triangulateFace(FaceHandle handle) const;

    // ---- Primitives (mesh_data_primitives.cpp) ----
    // Low-level pointer edits. The mesh may be left temporarily invalid;
    // operators are responsible for restoring it.

    VertexHandle addVertex(Vec3 position);

    // Creates origin -> tip and its pair, unlinked and with no face.
    // Returns the origin -> tip half-edge.
    EdgeHandle addEdgePair(VertexHandle origin, VertexHandle tip);

    // Removes both half-edges from storage without relinking anything.
    void deleteEdgePair(EdgeHandle handle);

    // a.next = b and b.prev = a.
    void link(EdgeHandle a, EdgeHandle b);

    // Links handle.prev to handle.next and moves face.edge off handle.
    // Does not touch the pair or delete anything.
    void spliceOut(EdgeHandle handle);

    // Sets .face on every edge in start's loop. A valid face gets start as its edge.
    void assignFace(EdgeHandle start, FaceHandle face);

    // Point a vertex or face at a valid edge again after edits.
    void repairVertexEdge(VertexHandle handle);
    void repairFaceEdge(FaceHandle handle);

    // Every edge pointing at `from` points at `to` instead.
    void retargetIncoming(VertexHandle from, VertexHandle to);

    // ---- Edge collapse (mesh_data_merge.cpp) ----

    // False if collapsing would leave duplicate edges or pinch two borders together.
    bool canCollapseEdge(EdgeHandle ab) const;

    // Merges ab's tip into its origin and moves the result to position.
    // Call canCollapseEdge first.
    void collapseEdge(EdgeHandle ab, Vec3 position);

    // Removes one half of the collapsing edge from its loop. A triangle on that
    // side is deleted and its apex is added to apexes.
    void collapseSide(EdgeHandle side, std::vector<VertexHandle>& apexes);

    // ---- Dissolve helpers (mesh_data_dissolve.cpp) ----

    // Removes an edge between two different faces and merges them into one.
    bool joinFaces(EdgeHandle handle);


    MeshArray<Vertex, VertexHandle> m_vertices;
    MeshArray<Edge, EdgeHandle> m_edges;
    MeshArray<Face, FaceHandle> m_faces;
    
    bool m_dirty = true;
};