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

// A run of indices whose faces share a material (INVALID_MATERIAL: the object's), drawn in one call
struct FaceGroup {
    MaterialHandle material = INVALID_MATERIAL;
    u32 firstIndex = 0;
    u32 indexCount = 0;
};

struct FaceData {
    static constexpr u32 FLOATS_PER_VERTEX = 8;

    std::vector<f32> vertices;    // x, y, z, nx, ny, nz, u, v per triangle corner
    std::vector<u32> indices;     // faces sorted by group, so each group's triangles are one run
    std::vector<u32> indexMap;    // per face slot: first index and index count
    std::vector<FaceGroup> groups;
};

// Which group a face's material puts it in; e.g. INVALID_MATERIAL for a material that was removed
using FaceGroupOf = std::function<MaterialHandle(MaterialHandle)>;

// Names the mesh's layout: any vertex, edge, or face added or removed, or a face material changed, gives a new stamp,
// and copies keep theirs. A GPU copy made at an equal stamp only needs moved positions sent again.
struct MeshStamp {
    u64 vertices = 0;
    u64 edges = 0;
    u64 faces = 0;
    u64 materials = 0;
    u64 uvs = 0;
    u64 shading = 0;

    bool operator==(const MeshStamp&) const = default;
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
    // The face's own material, or INVALID_MATERIAL when it uses the object's
    MaterialHandle getFaceMaterial(FaceHandle handle) const;
    void setFaceMaterial(FaceHandle handle, MaterialHandle material);
    // The material all of these faces share, or INVALID_MATERIAL when they differ (or there are none)
    MaterialHandle sharedFaceMaterial(const std::vector<FaceHandle>& faces) const;
    const std::vector<Triangle>& getFaceTriangles(FaceHandle handle) const;

    void positionVertex(VertexHandle handle, Vec3 position);
    void translateVertex(VertexHandle handle, Vec3 delta);

    // ---- UVs: each half-edge holds the UV of its face's corner at its tip ----

    // In getFaceVertices order
    std::vector<Vec2> getFaceUVs(FaceHandle handle) const;
    void setFaceUVs(FaceHandle handle, const std::vector<Vec2>& uvs);
    // Every half-edge's, in getEdgeHandles order (for saving)
    std::vector<Vec2> getCornerUVs() const;
    void setCornerUVs(const std::vector<Vec2>& uvs);
    // The faces connected to this one in UV space (its island), itself included: neighbours across edges whose two
    // sides have the same UVs at both ends, and theirs in turn
    std::vector<FaceHandle> getUVIsland(FaceHandle handle) const;

    // ---- UV seams: edges unwrapping cuts along; kept on both half-edges (mesh_data_access.cpp) ----

    bool isSeam(EdgeHandle handle) const;
    void setSeam(EdgeHandle handle, bool seam);
    // One half of each seam
    std::vector<EdgeHandle> getSeamEdges() const;
    // The seam flags of the edges ending at each corner, in getFaceVertices order
    std::vector<bool> getFaceEdgeSeams(FaceHandle handle) const;
    // Every half-edge's, in getEdgeHandles order (for saving)
    std::vector<bool> getEdgeSeams() const;
    void setEdgeSeams(const std::vector<bool>& seams);
    bool hasSeams() const;
    // Marks a seam on every edge between two faces whose UVs differ there, so the current layout's cuts become seams;
    // returns how many edges it marked
    u32 markSeamsFromIslands();

    // ---- Smooth shading (mesh_data_shading.cpp) ----

    ShadingMode getShading() const { return m_shading; }
    void setShading(ShadingMode mode);
    // Auto mode: faces meeting at more than this (radians) keep a hard edge
    f32 getSmoothAngle() const { return m_smoothAngle; }
    void setSmoothAngle(f32 radians);
    EdgeMark getEdgeMark(EdgeHandle handle) const;
    // Sets both half-edges
    void setEdgeMark(EdgeHandle handle, EdgeMark mark);
    // The marks of the edges ending at each corner, in getFaceVertices order
    std::vector<EdgeMark> getFaceEdgeMarks(FaceHandle handle) const;
    // Every half-edge's, in getEdgeHandles order (for saving)
    std::vector<EdgeMark> getEdgeMarks() const;
    void setEdgeMarks(const std::vector<EdgeMark>& marks);
    bool hasEdgeMarks() const;
    // Whether the faces either side of the edge shade apart: always on a border or in flat mode, then the mark, then the mode
    bool isEdgeHard(EdgeHandle handle) const;
    // One half of each edge that shades hard between two faces; empty in flat mode
    std::vector<EdgeHandle> getHardEdges() const;
    // The normal of the corner at incoming's tip in its face: in flat mode the face's, otherwise the faces around
    // that vertex up to the nearest hard edges, weighted by their corner angles
    Vec3 getCornerNormal(EdgeHandle incoming) const;
    // Within this fraction of its size, every corner lies on the face's plane
    bool isFacePlanar(FaceHandle handle) const;
    // Faces whose corners a move of these vertices changes: the faces around them, and with smoothing their neighbours too
    std::vector<FaceHandle> getFacesToPatch(const std::vector<VertexHandle>& moved) const;

    // ---- Changes since the GPU copy was last brought up to date (see MeshStamp) ----

    MeshStamp stamp() const;
    // Vertices whose positions changed (possibly repeated), unless so many changed that allMoved says all of them
    const std::vector<VertexHandle>& movedVertices() const { return m_moved; }
    bool allMoved() const { return m_allMoved; }
    // After the mesh is replaced by a copy (undo, a cancelled tool): positions may differ from what was drawn last
    // without any vertex having been moved through this mesh
    void markAllMoved() { m_moved.clear(); m_allMoved = true; }
    void clearMoved();
    // Every face using the vertex
    std::vector<FaceHandle> getVertexFaces(VertexHandle handle) const;
    // The face's corners as the GPU draws them: three per triangle, x, y, z, nx, ny, nz, u, v each (see getFaceData)
    void appendFaceCorners(FaceHandle handle, std::vector<f32>& out) const;

    void setFacesDirtyByVertex(VertexHandle handle) const;
    void setFacesDirtyByEdge(EdgeHandle handle) const;
    void setFacesDirtyByFace(FaceHandle handle) const;

    // ---- Operators (mesh_data_ops.cpp, mesh_data_merge.cpp, mesh_data_dissolve.cpp) ----

    FaceHandle insertFaceRing(FaceHandle handle);
    VertexHandle splitEdge(EdgeHandle handle);

    bool removeVertex(VertexHandle handle);
    bool removeEdge(EdgeHandle handle);
    bool removeFace(FaceHandle handle);

    // The new face takes the material its neighbors across the loop share (the object's when they differ), or
    // material when one is given
    // Its corners take the UVs of the faces across the loop, or uvByVertex (vertex slot to UV) when given
    bool fillFaceLoop(EdgeHandle handle, const MaterialHandle* material = nullptr, const std::unordered_map<u32, Vec2>* uvByVertex = nullptr);
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
    // Without groupOf, faces group by their own material handle as stored
    FaceData getFaceData(const FaceGroupOf& groupOf = {}) const;

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
    // The first oldFaces.size() new faces replace the old ones in order and keep their materials; the rest are new
    // and take the material the old faces share (the object's when they differ)
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

    // Positions changed since the last clearMoved, and face material changes (see MeshStamp)
    void recordMoved(VertexHandle handle);
    std::vector<VertexHandle> m_moved;
    bool m_allMoved = false;
    u64 m_materialStamp = nextStructureStamp();
    u64 m_uvStamp = nextStructureStamp();

    ShadingMode m_shading = ShadingMode::Flat;
    f32 m_smoothAngle = 0.5235988f;   // 30 degrees
    u64 m_shadingStamp = nextStructureStamp();
};