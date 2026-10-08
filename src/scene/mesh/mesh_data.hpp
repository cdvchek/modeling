#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <types>

#include "scene/mesh/mesh_types.hpp"
#include "scene/mesh/mesh_handles.hpp"
#include "core/math/vec2.hpp"
#include "core/math/mat4.hpp"
#include "core/io/binary_io.hpp"

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

struct SlideSession {
    DynamicArray<Vertex, VertexHandle> savedVertices;
    DynamicArray<Edge, EdgeHandle> savedEdges;
    DynamicArray<Face, FaceHandle> savedFaces;

    std::vector<VertexHandle> vertices;
    std::vector<Vec3> starts;
    std::vector<Vec3> directions;

    f32 maxWidth = 0.0f;
};

// Why extrude or inset refused a face selection
enum class RegionError : u8 {
    None,
    NoFaces,
    CornerTouch,    // regions (or one region with itself) meet only at a vertex
    NoBoundary,     // the region is a closed surface
    Holes,          // the region's boundary is more than one loop
    Failed          // rebuilding the faces failed; the mesh must be restored
};

const char* regionErrorText(RegionError error);

class MeshData {
public:
    // ---- Access (mesh_data_access.cpp) ----

    void setMesh(PresetMesh meshType);
    // Takes a mesh built elsewhere (MeshFactory::fromPolygons, an imported asset)
    void setMesh(const PackagedMesh& mesh);

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

    // space is where the bevel is measured (e.g. the object's matrix for world space); widths are in its units
    bool bevelVertex(VertexHandle handle, SlideSession& session, const Mat4& space = Mat4::identity());
    bool bevelEdge(EdgeHandle handle, SlideSession& session, const Mat4& space = Mat4::identity());
    bool bevelFace(FaceHandle handle, SlideSession& session, const Mat4& space = Mat4::identity());

    // ---- Regions (mesh_data_region.cpp) ----

    // Extrude and inset work on regions: selected faces joined by shared edges, each needing one simple boundary loop.
    // Both duplicate each region's boundary, move the region's faces onto the copies, and join old and new boundaries with quads.
    // Extrude leaves the copies in place (topFaces are the moved faces); inset slides them inward by a width (innerFaces).
    RegionError extrudeRegions(const std::vector<FaceHandle>& faces, std::vector<FaceHandle>& topFaces);
    RegionError insetRegions(const std::vector<FaceHandle>& faces, SlideSession& session, std::vector<FaceHandle>& innerFaces, const Mat4& space = Mat4::identity());

    void setSlideWidth(const SlideSession& session, f32 width);
    void cancelSlide(const SlideSession& session);


    // ---- Files (mesh_data_serialize.cpp) ----

    // Writes the half-edge structure with deleted slots packed out, so handles are renumbered from 0
    void writeTo(BinaryWriter& writer) const;
    // Replaces this mesh with one written by writeTo; false (mesh unchanged) if the data is cut short or links out of range
    bool readFrom(BinaryReader& reader);

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
        SlideSession& session
    );

    // Runs op with every position moved into space, then brings positions and the session's slides back to mesh space
    bool runInSpace(SlideSession& session, const Mat4& space, const std::function<bool()>& op);

    // createdFaces, if given, receives the new faces in the order of newFaces.
    // With allowBorders, a new edge with nothing on its other side becomes a mesh border, and old border edges
    // that nothing uses anymore are removed; otherwise either case fails.
    bool replaceFaces(
        const std::vector<FaceHandle>& oldFaces,
        const std::vector<std::vector<VertexHandle>>& newFaces,
        std::vector<FaceHandle>* createdFaces = nullptr,
        bool allowBorders = false
    );

    // Links every border half-edge to the border half-edge leaving its tip
    void relinkBorders();

    // ---- Region helpers (mesh_data_region.cpp) ----

    struct Region {
        std::vector<FaceHandle> faces;
        std::vector<EdgeHandle> boundary;   // one loop, in the region faces' winding
    };

    RegionError findRegions(const std::vector<FaceHandle>& faces, std::vector<Region>& regions) const;

    // originals[i] is a boundary vertex and copies[i] the new vertex replacing it in the region's faces
    bool ringRegions(
        const std::vector<Region>& regions,
        std::vector<FaceHandle>& topFaces,
        std::vector<VertexHandle>& originals,
        std::vector<VertexHandle>& copies
    );

    // ---- Dissolve helpers (mesh_data_dissolve.cpp) ----

    bool joinFaces(EdgeHandle handle);


    DynamicArray<Vertex, VertexHandle> m_vertices;
    DynamicArray<Edge, EdgeHandle> m_edges;
    DynamicArray<Face, FaceHandle> m_faces;
    
    bool m_dirty = true;
};