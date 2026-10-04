#pragma once

#include <vector>
#include <unordered_map>
#include <types>

#include "scene/mesh/mesh_types.hpp"
#include "scene/mesh/mesh_handles.hpp"
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
    static constexpr u32 FLOATS_PER_VERTEX = 6;

    std::vector<f32> vertices;    // x, y, z, nx, ny, nz per triangle corner
    std::vector<u32> indices;
    std::vector<u32> indexMap;
};

struct BevelSession {
    DynamicArray<Vertex, VertexHandle> savedVertices;
    DynamicArray<Edge, EdgeHandle> savedEdges;
    DynamicArray<Face, FaceHandle> savedFaces;

    std::vector<VertexHandle> vertices;
    std::vector<Vec3> starts;
    std::vector<Vec3> directions;

    f32 maxWidth = 0.0f;
};

class MeshData {
public:
    // ---- Access (mesh_data_access.cpp) ----

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

    std::vector<VertexHandle> getFaceVertices(FaceHandle handle) const;

    std::vector<EdgeHandle> getOutgoingEdges(VertexHandle handle) const;
    std::vector<EdgeHandle> getIncomingEdges(VertexHandle handle) const;
    std::vector<EdgeHandle> getLoopEdges(EdgeHandle start) const;

    bool isBorder(EdgeHandle handle) const;
    bool isBorderVertex(VertexHandle handle) const;

    std::vector<EdgeHandle> getEdgeLoop(EdgeHandle start) const;
    std::vector<EdgeHandle> getEdgeRing(EdgeHandle start) const;
    std::vector<FaceHandle> getFaceLoop(EdgeHandle start) const;

    bool validate() const;

    // ---- Geometry (mesh_data_geometry.cpp) ----

    Vec3 getVertexPosition(VertexHandle handle) const;
    Vec3 getFaceNormal(FaceHandle handle) const;
    const std::vector<Triangle>& getFaceTriangles(FaceHandle handle) const;

    void positionVertex(VertexHandle handle, Vec3 position);
    void translateVertex(VertexHandle handle, Vec3 delta);

    void setFacesDirtyByVertex(VertexHandle handle) const;
    void setFacesDirtyByEdge(EdgeHandle handle) const;
    void setFacesDirtyByFace(FaceHandle handle) const;

    // ---- Operators (mesh_data_ops.cpp, mesh_data_merge.cpp, mesh_data_dissolve.cpp) ----

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

    // ---- Bevel (mesh_data_bevel.cpp) ----

    bool bevelVertex(VertexHandle handle, BevelSession& session);
    bool bevelEdge(EdgeHandle handle, BevelSession& session);
    bool bevelFace(FaceHandle handle, BevelSession& session);

    void setBevelWidth(const BevelSession& session, f32 width);
    void cancelBevel(const BevelSession& session);


    // ---- GPU export (mesh_data_gpu.cpp) ----

    VertexData getVertexData() const;
    EdgeData getEdgeData(const VertexData& vertexData) const;
    FaceData getFaceData() const;

private:
    // ---- Topology queries (mesh_data_queries.cpp) ----

    std::vector<EdgeHandle> getFaceEdges(FaceHandle handle) const;
    std::vector<VertexHandle> getVertexNeighbors(VertexHandle handle) const;
    EdgeHandle findEdge(VertexHandle origin, VertexHandle tip) const;
    void walkRing(EdgeHandle start, std::vector<EdgeHandle>& edges, std::vector<FaceHandle>& faces) const;

    // ---- Geometry (mesh_data_geometry.cpp) ----

    std::vector<Triangle> triangulateFace(FaceHandle handle) const;

    // ---- Primitives (mesh_data_primitives.cpp) ----

    VertexHandle addVertex(Vec3 position);

    EdgeHandle addEdgePair(VertexHandle origin, VertexHandle tip);

    void deleteEdgePair(EdgeHandle handle);

    void link(EdgeHandle a, EdgeHandle b);

    void spliceOut(EdgeHandle handle);

    void assignFace(EdgeHandle start, FaceHandle face);

    void repairVertexEdge(VertexHandle handle);
    void repairFaceEdge(FaceHandle handle);

    void retargetIncoming(VertexHandle from, VertexHandle to);

    // ---- Edge collapse (mesh_data_merge.cpp) ----

    bool canCollapseEdge(EdgeHandle ab) const;

    void collapseEdge(EdgeHandle ab, Vec3 position);

    void collapseSide(EdgeHandle side, std::vector<VertexHandle>& apexes);

    // ---- Bevel helpers (mesh_data_bevel.cpp) ----

    bool bevel(
        const std::vector<VertexHandle>& corners,
        const std::vector<EdgeHandle>& edges,
        bool vertexOnly,
        BevelSession& session
    );

    bool replaceFaces(
        const std::vector<FaceHandle>& oldFaces,
        const std::vector<std::vector<VertexHandle>>& newFaces
    );

    // ---- Dissolve helpers (mesh_data_dissolve.cpp) ----

    bool joinFaces(EdgeHandle handle);


    DynamicArray<Vertex, VertexHandle> m_vertices;
    DynamicArray<Edge, EdgeHandle> m_edges;
    DynamicArray<Face, FaceHandle> m_faces;
    
    bool m_dirty = true;
};