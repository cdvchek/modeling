#pragma once

#include <types>
#include <vector>
#include <unordered_map>

#include "scene/mesh/mesh_handles.hpp"
#include "scene/mesh/mesh_data.hpp"
#include "renderer/gpu_mesh.hpp"

class OpenGLMesh : public IMesh {
public:
    // groupOf says which material group each face's triangles go in (see MeshData::getFaceData); groupingStamp names
    // what that grouping was decided against (the material collection's stamp), so a later change to it rebuilds
    bool create(const MeshData& mesh, const FaceGroupOf& groupOf = {}, u64 groupingStamp = 0);
    bool update(const MeshData& mesh, const FaceGroupOf& groupOf = {}, u64 groupingStamp = 0);
    void destroy();

    // Brings the buffers up to date after vertices only moved: the moved positions and the corners of every face
    // around them, sent as one changed range per buffer. False, changing nothing that matters, when the layout
    // changed since the last upload (call update instead).
    bool patch(const MeshData& mesh, u64 groupingStamp);

    void drawVertices() const override;
    void drawEdges() const override;
    void drawFaces() const override;

    const std::vector<FaceGroup>& faceGroups() const override { return m_faceGroups; }
    void drawFaceRange(u32 firstIndex, u32 indexCount) const override;

    void drawVertexSet(const std::vector<VertexHandle>& vertices) override;
    void drawEdgeSet(const std::vector<EdgeHandle>& edges) override;
    void drawHardEdgeSet(const std::vector<EdgeHandle>& edges) override;
    void drawFaceSet(const std::vector<FaceHandle>& faces) override;

private:
    // Indices for one selection set, drawn through its own vertex array so the main ones keep their buffers
    struct ElementSet {
        u32 vao = 0;
        u32 ebo = 0;
        u32 count = 0;
        std::vector<u32> key;       // the handles it was built from (index, generation pairs)
        bool built = false;
    };

    void upload(const MeshData& mesh, const FaceGroupOf& groupOf, u64 groupingStamp);
    void createSet(ElementSet& set, u32 vbo, bool faceLayout);
    // Uploads indices for a set unless it was built from the same handles; returns false when there's nothing to draw
    bool prepareSet(ElementSet& set, const std::vector<u32>& key, const std::vector<u32>& indices);
    void destroySet(ElementSet& set);
    void drawEdgesIn(ElementSet& set, const std::vector<EdgeHandle>& edges);

    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_edgeEbo = 0;

    u32 m_faceVao = 0;
    u32 m_faceVbo = 0;
    u32 m_faceEbo = 0;

    u32 m_vCount = 0;
    u32 m_eIndCount = 0;
    u32 m_fIndCount = 0;

    std::vector<u32> m_vertexIndexMap;
    std::unordered_map<u32, u32> m_edgeIndexMap;
    std::vector<u32> m_edgeIndices;
    std::vector<u32> m_faceIndexMap;
    std::vector<FaceGroup> m_faceGroups;

    // What was last uploaded, kept to patch in place: the layout's stamps, and the buffers' contents
    MeshStamp m_stamp;
    u64 m_groupingStamp = 0;
    std::vector<f32> m_positions;
    std::vector<f32> m_faceVertices;

    ElementSet m_vertexSet;
    ElementSet m_edgeSet;
    ElementSet m_hardEdgeSet;
    ElementSet m_faceSet;

    bool m_initialized = false;
};
