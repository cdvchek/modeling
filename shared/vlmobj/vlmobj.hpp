#pragma once

// The .vlmobj asset format, shared by Valuma Studio (which writes it) and the Aevora engine (which loads it).
// Spec: docs/systems/vlmobj.md. Depends only on the C++ standard library.
//
// Reading: File views a buffer in place (a memory-mapped file works): open() checks it, then nodes(), meshes(),
// parts(), and the vertex and index data are pointers into the buffer. Writing: Writer collects nodes and meshes
// and produces the bytes.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vlmobj {
    static_assert(std::endian::native == std::endian::little, ".vlmobj is little-endian; a big-endian reader needs byte swapping");

    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using f32 = float;

    inline constexpr char MAGIC[4] = { 'V', 'L', 'M', 'O' };
    inline constexpr u32 VERSION = 1;
    inline constexpr u32 HEADER_SIZE = 64;
    inline constexpr u32 SECTION_ALIGNMENT = 64;
    inline constexpr u32 DATA_ALIGNMENT = 16;
    inline constexpr u32 MAX_ATTRIBUTES = 8;

    // An empty index (no parent, no mesh, default material)
    inline constexpr u32 NONE = 0xFFFFFFFFu;

    // Four characters as a u32 with the first in the low byte, so it reads as text in a hex dump
    constexpr u32 fourCC(const char (&name)[5]) {
        return u32(u8(name[0])) | u32(u8(name[1])) << 8 | u32(u8(name[2])) << 16 | u32(u8(name[3])) << 24;
    }

    namespace Section {
        inline constexpr u32 STRINGS = fourCC("STRS");
        inline constexpr u32 NODES = fourCC("NODE");
        inline constexpr u32 MESHES = fourCC("MESH");
        inline constexpr u32 PARTS = fourCC("PART");
        inline constexpr u32 VERTICES = fourCC("VTXS");
        inline constexpr u32 INDICES = fourCC("IDXS");
        inline constexpr u32 EDIT = fourCC("EDIT");
        inline constexpr u32 MATERIALS = fourCC("MATL");

        // Reserved for later versions
        inline constexpr u32 TEXTURES = fourCC("TEXR");
        inline constexpr u32 SKELETON = fourCC("SKEL");
        inline constexpr u32 ANIMATIONS = fourCC("ANIM");
        inline constexpr u32 ANIMATION_EVENTS = fourCC("AEVT");
        inline constexpr u32 COLLISION = fourCC("COLL");
        inline constexpr u32 ATTACHMENT_POINTS = fourCC("ATTP");
        inline constexpr u32 LEVELS_OF_DETAIL = fourCC("LODS");
    }

    // Directory entry flags
    inline constexpr u8 SECTION_EDITOR_ONLY = 1 << 0;

    enum class Compression : u8 { None = 0 };

    enum class Semantic : u8 {
        None = 0, Position = 1, Normal = 2, Tangent = 3, UV0 = 4, UV1 = 5, Color = 6, Joints = 7, Weights = 8
    };

    enum class Format : u8 {
        None = 0, F32x2 = 1, F32x3 = 2, F32x4 = 3, F16x2 = 4, Snorm16x2 = 5, Unorm8x4 = 6, U8x4 = 7, U16x4 = 8, Unorm16x4 = 9
    };

    // Bytes one value of the format takes, or 0 for an unknown format
    u32 formatSize(Format format);

    // ---- Records (layouts fixed by the spec; the asserts below keep them that way) ----

    struct Header {
        char magic[4];
        u32 version;
        u32 headerSize;
        u32 flags;
        u32 sectionCount;
        u32 directoryCrc;
        u64 directoryOffset;
        u64 fileSize;
        f32 metersPerUnit;
        u8 upAxis;          // 1 = +Y
        u8 handedness;      // 0 = right-handed
        u16 reserved0;
        u32 headerCrc;      // CRC-32 of bytes 0-47
        u8 reserved1[12];
    };

    struct DirectoryEntry {
        u32 type;
        u32 version;
        u64 offset;
        u64 storedSize;
        u64 size;
        u32 crc;
        u8 compression;
        u8 flags;
        u16 reserved0;
        u32 count;
        u32 reserved1;
    };

    struct StringRef {
        u32 offset;
        u32 length;
    };

    struct Node {
        StringRef name;
        u32 parent;
        u32 mesh;
        f32 translation[3];
        f32 rotation[4];    // unit quaternion x, y, z, w
        f32 scale[3];
        u32 flags;
        u32 reserved;
    };

    struct VertexAttribute {
        u8 semantic;
        u8 format;
        u16 reserved;
        u32 offset;
    };

    struct Mesh {
        StringRef name;
        u32 vertexCount;
        u32 indexCount;
        u64 vertexOffset;   // into the VTXS section
        u64 indexOffset;    // into the IDXS section
        u32 vertexStride;
        u8 indexSize;       // 2 or 4
        u8 attributeCount;
        u8 primitive;       // 0 = triangle list
        u8 reserved0;
        u32 firstPart;
        u32 partCount;
        f32 boundsMin[3];
        f32 boundsMax[3];
        f32 sphereCenter[3];
        f32 sphereRadius;
        VertexAttribute attributes[MAX_ATTRIBUTES];
        u8 reserved1[8];
    };

    struct Part {
        u32 firstIndex;
        u32 indexCount;
        u32 material;       // into the MATL section, or NONE for the engine's default material
        u32 reserved;
    };

    enum class AlphaMode : u8 { Opaque = 0, Cutout = 1, Blend = 2 };

    inline constexpr u8 MATERIAL_DOUBLE_SIDED = 1 << 0;

    // Metallic-roughness, as glTF. Colors are sRGB as authored (convert to linear when loading), each 0 to 1.
    struct Material {
        StringRef name;
        f32 baseColor[3];
        f32 roughness;
        f32 metallic;
        f32 emissiveColor[3];
        f32 emissiveStrength;   // multiplies emissiveColor; 0 doesn't glow
        f32 opacity;
        f32 alphaCutoff;        // Cutout: drawn where opacity reaches it
        u8 alphaMode;           // AlphaMode
        u8 flags;               // MATERIAL_DOUBLE_SIDED
        u16 reserved0;
        // Version 2: into the TEXR section, or NONE; multiplies baseColor (its alpha multiplies opacity). Reserved (0)
        // in version 1, which means none: read it with File::baseColorTexture
        u32 baseColorTexture;
        u32 reserved1;
    };

    // How a texture's pixels are stored
    enum class TextureFormat : u8 { Png = 1 };

    // A picture materials use, read through the mesh's UVs (uv0). The data sits in the TEXR section after the
    // records: a whole PNG file for TextureFormat::Png. Colors are sRGB.
    struct Texture {
        StringRef name;
        u32 width;              // pixels
        u32 height;
        u8 format;              // TextureFormat
        u8 flags;
        u16 reserved0;
        u32 reserved1;
        u64 dataOffset;         // from the start of the TEXR section, 16-byte aligned
        u64 dataSize;
        u32 reserved2[6];
    };

    static_assert(sizeof(Header) == 64 && offsetof(Header, directoryOffset) == 24 && offsetof(Header, headerCrc) == 48);
    static_assert(sizeof(DirectoryEntry) == 48 && offsetof(DirectoryEntry, crc) == 32 && offsetof(DirectoryEntry, count) == 40);
    static_assert(sizeof(StringRef) == 8);
    static_assert(sizeof(Node) == 64 && offsetof(Node, translation) == 16 && offsetof(Node, rotation) == 28 && offsetof(Node, flags) == 56);
    static_assert(sizeof(VertexAttribute) == 8);
    static_assert(sizeof(Mesh) == 160 && offsetof(Mesh, vertexStride) == 32 && offsetof(Mesh, boundsMin) == 48 && offsetof(Mesh, attributes) == 88);
    static_assert(sizeof(Part) == 16);
    static_assert(sizeof(Material) == 64 && offsetof(Material, roughness) == 20 && offsetof(Material, emissiveStrength) == 40 && offsetof(Material, alphaMode) == 52
                  && offsetof(Material, baseColorTexture) == 56);
    static_assert(sizeof(Texture) == 64 && offsetof(Texture, format) == 16 && offsetof(Texture, dataOffset) == 24);

    // Standard CRC-32 (zlib/PNG)
    u32 crc32(const void* data, std::size_t size);

    // ---- Node transforms ----

    struct NodeTransform {
        f32 translation[3] = { 0.0f, 0.0f, 0.0f };
        f32 rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };   // unit quaternion x, y, z, w
        f32 scale[3] = { 1.0f, 1.0f, 1.0f };
    };

    NodeTransform nodeTransform(const Node& node);

    // A node's world transform from its parent's world transform and its own, with no skew: the rotations combine,
    // the parent's scale multiplies the node's along the node's own axes, and the translation is placed in the
    // parent (scaled, turned, then moved by it). This is the rule every reader must use (see the spec).
    NodeTransform combine(const NodeTransform& parent, const NodeTransform& local);

    // ---- Editable polygons (the EDIT section) ----

    struct EditMesh {
        std::vector<f32> positions;     // x, y, z per vertex
        std::vector<u32> faceSizes;     // corners per face
        std::vector<u32> corners;       // vertex indices, face after face, counter-clockwise from the front
        // Version 2: the object's material and each face's own (empty when no face has one); MATL indices or NONE
        u32 material = NONE;
        std::vector<u32> faceMaterials;
        // Version 3: each corner's UV (u, v), in corner order; empty for none
        std::vector<f32> uvs;
        // Version 4: shading (0 flat, 1 smooth, 2 auto), the auto angle in radians, and the mark of the edge
        // ending at each corner (0 none, 1 hard, 2 smooth), in corner order; empty for none
        u32 shading = 0;
        f32 smoothAngle = 0.5235988f;
        std::vector<u8> edgeMarks;
    };

    struct EditData {
        std::vector<f32> eulerRotations;    // x, y, z per node, radians, applied X then Y then Z
        std::vector<EditMesh> meshes;       // one per mesh, in mesh order
    };

    // ---- Reading ----

    struct ReadOptions {
        bool verifyChecksums = true;   // CRC every section (reads every byte)
        bool verifyIndices = true;     // every index below its mesh's vertex count (reads every index)
    };

    class File {
    public:
        // Checks the buffer and keeps a view of it; the buffer must outlive the File and start on an 8-byte boundary.
        // On failure, error() says why.
        bool open(const void* data, std::size_t size, const ReadOptions& options = {});

        const char* error() const { return m_error.c_str(); }

        const Header& header() const { return *reinterpret_cast<const Header*>(m_data); }
        std::span<const DirectoryEntry> sections() const { return m_sections; }
        // The first section of a type, or null
        const DirectoryEntry* find(u32 type) const;
        const u8* data(const DirectoryEntry& entry) const { return m_data + entry.offset; }

        std::span<const Node> nodes() const { return m_nodes; }
        std::span<const Mesh> meshes() const { return m_meshes; }
        std::span<const Part> parts() const { return m_parts; }
        std::span<const Material> materials() const { return m_materials; }
        std::span<const Texture> textures() const { return m_textures; }
        // The material's base color texture, NONE when it has none (always NONE in a version 1 MATL section)
        u32 baseColorTexture(const Material& material) const { return m_materialVersion >= 2 ? material.baseColorTexture : NONE; }
        // A texture's stored bytes (the PNG file for TextureFormat::Png)
        std::span<const u8> textureData(const Texture& texture) const { return { m_textureSection + texture.dataOffset, static_cast<std::size_t>(texture.dataSize) }; }

        // A mesh's vertices (vertexCount × vertexStride bytes) and indices (indexCount × indexSize bytes)
        const u8* vertexData(const Mesh& mesh) const { return m_vertices + mesh.vertexOffset; }
        const u8* indexData(const Mesh& mesh) const { return m_indices + mesh.indexOffset; }
        // Every mesh's vertex or index data in one block, for a single upload
        std::span<const u8> allVertexData() const { return { m_vertices, m_vertexBytes }; }
        std::span<const u8> allIndexData() const { return { m_indices, m_indexBytes }; }

        std::string_view string(StringRef ref) const;

        // Every node's world transform, in node order (parents come first, so one pass does it)
        std::vector<NodeTransform> worldTransforms() const;

        // Reads the EDIT section; false if there is none or it doesn't match the meshes
        bool readEditData(EditData& out) const;

    private:
        bool fail(const std::string& message);
        bool checkMeshes(const ReadOptions& options);

        const u8* m_data = nullptr;
        std::size_t m_size = 0;
        std::span<const DirectoryEntry> m_sections;
        std::span<const Node> m_nodes;
        std::span<const Mesh> m_meshes;
        std::span<const Part> m_parts;
        std::span<const Material> m_materials;
        u32 m_materialVersion = 0;
        std::span<const Texture> m_textures;
        const u8* m_textureSection = nullptr;
        const u8* m_strings = nullptr;
        u64 m_stringBytes = 0;
        const u8* m_vertices = nullptr;
        u64 m_vertexBytes = 0;
        const u8* m_indices = nullptr;
        u64 m_indexBytes = 0;
        std::string m_error;
    };

    // ---- Writing ----

    struct NodeInput {
        std::string name;
        u32 parent = NONE;
        u32 mesh = NONE;
        f32 translation[3] = { 0.0f, 0.0f, 0.0f };
        f32 rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        f32 scale[3] = { 1.0f, 1.0f, 1.0f };
    };

    struct PartInput {
        u32 firstIndex = 0;
        u32 indexCount = 0;
        u32 material = NONE;
    };

    struct MaterialInput {
        std::string name;
        f32 baseColor[3] = { 1.0f, 1.0f, 1.0f };
        f32 roughness = 0.5f;
        f32 metallic = 0.0f;
        f32 emissiveColor[3] = { 1.0f, 1.0f, 1.0f };
        f32 emissiveStrength = 0.0f;
        f32 opacity = 1.0f;
        f32 alphaCutoff = 0.5f;
        AlphaMode alphaMode = AlphaMode::Opaque;
        bool doubleSided = false;
        u32 baseColorTexture = NONE;    // from addTexture
    };

    struct TextureInput {
        std::string name;
        u32 width = 0;
        u32 height = 0;
        std::vector<u8> png;            // the whole PNG file
    };

    struct MeshInput {
        std::string name;
        std::vector<VertexAttribute> attributes;   // must include a Position in F32x3 (used for the bounds)
        u32 vertexStride = 0;
        std::vector<u8> vertices;                  // vertexCount × vertexStride bytes
        std::vector<u32> indices;                  // stored as u16 when the mesh has at most 65,536 vertices
        std::vector<PartInput> parts;              // empty: one part over every index, default material
    };

    class Writer {
    public:
        // Returns the node's index; nodes must be added parents first, the root (parent NONE) first of all
        u32 addNode(const NodeInput& node);
        u32 addMesh(MeshInput mesh);
        // Returns the material's index, for parts to point at
        u32 addMaterial(const MaterialInput& material);
        // Returns the texture's index, for materials to point at
        u32 addTexture(TextureInput texture);
        void setEditData(EditData edit);

        // The finished file. Deterministic: the same input always gives the same bytes.
        std::vector<u8> finish() const;

    private:
        std::vector<NodeInput> m_nodes;
        std::vector<MeshInput> m_meshes;
        std::vector<MaterialInput> m_materials;
        std::vector<TextureInput> m_textures;
        EditData m_edit;
        bool m_hasEdit = false;
    };
}
