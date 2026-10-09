#include "vlmobj/vlmobj.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>

namespace vlmobj {
    namespace {
        constexpr std::array<u32, 256> makeCrcTable() {
            std::array<u32, 256> table {};
            for (u32 i = 0; i < 256; ++i) {
                u32 value = i;
                for (int bit = 0; bit < 8; ++bit) value = (value & 1) ? (value >> 1) ^ 0xEDB88320u : value >> 1;
                table[i] = value;
            }
            return table;
        }

        constexpr std::array<u32, 256> CRC_TABLE = makeCrcTable();

        // The newest version of each section this code reads and writes
        u32 supportedVersion(u32 type) {
            switch (type) {
                case Section::STRINGS:
                case Section::NODES:
                case Section::MESHES:
                case Section::PARTS:
                case Section::VERTICES:
                case Section::INDICES:
                case Section::TEXTURES:
                    return 1;
                case Section::MATERIALS:
                    return 2;
                case Section::EDIT:
                    return 4;
                default:
                    return 0;
            }
        }

        std::string typeName(u32 type) {
            std::string name;
            for (int i = 0; i < 4; ++i) {
                const char c = static_cast<char>((type >> (i * 8)) & 0xFF);
                name += (c >= 32 && c < 127) ? c : '?';
            }
            return name;
        }

        u64 aligned(u64 value, u64 alignment) {
            return (value + alignment - 1) / alignment * alignment;
        }

        // offset + size fits inside total, without overflowing
        bool fits(u64 offset, u64 size, u64 total) {
            return offset <= total && size <= total - offset;
        }

        template <typename T>
        void append(std::vector<u8>& bytes, const T& value) {
            const std::size_t at = bytes.size();
            bytes.resize(at + sizeof(T));
            std::memcpy(bytes.data() + at, &value, sizeof(T));
        }

        template <typename T>
        void appendArray(std::vector<u8>& bytes, const std::vector<T>& values) {
            if (values.empty()) return;
            const std::size_t at = bytes.size();
            bytes.resize(at + values.size() * sizeof(T));
            std::memcpy(bytes.data() + at, values.data(), values.size() * sizeof(T));
        }

        // Reads plain values from a byte range; any read past the end fails
        struct Cursor {
            const u8* data;
            std::size_t size;
            std::size_t position = 0;

            template <typename T>
            bool read(T& value) {
                if (sizeof(T) > size - position) return false;
                std::memcpy(&value, data + position, sizeof(T));
                position += sizeof(T);
                return true;
            }

            template <typename T>
            bool readArray(std::vector<T>& values, u64 count) {
                if (count > (size - position) / sizeof(T)) return false;
                values.resize(static_cast<std::size_t>(count));
                if (count > 0) std::memcpy(values.data(), data + position, static_cast<std::size_t>(count) * sizeof(T));
                position += static_cast<std::size_t>(count) * sizeof(T);
                return true;
            }
        };

        std::vector<u8> encodeEdit(const EditData& edit) {
            std::vector<u8> bytes;
            append(bytes, static_cast<u32>(edit.eulerRotations.size() / 3));
            appendArray(bytes, edit.eulerRotations);
            append(bytes, static_cast<u32>(edit.meshes.size()));

            for (const EditMesh& mesh : edit.meshes) {
                append(bytes, static_cast<u32>(mesh.positions.size() / 3));
                append(bytes, static_cast<u32>(mesh.faceSizes.size()));
                append(bytes, static_cast<u32>(mesh.corners.size()));
                appendArray(bytes, mesh.positions);
                appendArray(bytes, mesh.faceSizes);
                appendArray(bytes, mesh.corners);
                // Version 2
                append(bytes, mesh.material);
                append(bytes, static_cast<u32>(mesh.faceMaterials.size()));
                appendArray(bytes, mesh.faceMaterials);
                // Version 3
                append(bytes, static_cast<u32>(mesh.uvs.size() / 2));
                appendArray(bytes, mesh.uvs);
                // Version 4
                append(bytes, mesh.shading);
                append(bytes, mesh.smoothAngle);
                append(bytes, static_cast<u32>(mesh.edgeMarks.size()));
                appendArray(bytes, mesh.edgeMarks);
            }

            return bytes;
        }

        const VertexAttribute* findAttribute(const std::vector<VertexAttribute>& attributes, Semantic semantic) {
            for (const VertexAttribute& attribute : attributes) {
                if (attribute.semantic == static_cast<u8>(semantic)) return &attribute;
            }
            return nullptr;
        }
    }

    u32 formatSize(Format format) {
        switch (format) {
            case Format::F32x2: return 8;
            case Format::F32x3: return 12;
            case Format::F32x4: return 16;
            case Format::F16x2: return 4;
            case Format::Snorm16x2: return 4;
            case Format::Unorm8x4: return 4;
            case Format::U8x4: return 4;
            case Format::U16x4: return 8;
            case Format::Unorm16x4: return 8;
            default: return 0;
        }
    }

    u32 crc32(const void* data, std::size_t size) {
        const u8* bytes = static_cast<const u8*>(data);
        u32 crc = 0xFFFFFFFFu;
        for (std::size_t i = 0; i < size; ++i) crc = (crc >> 8) ^ CRC_TABLE[(crc ^ bytes[i]) & 0xFF];
        return crc ^ 0xFFFFFFFFu;
    }

    // ---- Node transforms ----

    namespace {
        // Hamilton product a * b (b's rotation first, then a's)
        void quaternionMultiply(const f32 a[4], const f32 b[4], f32 out[4]) {
            const f32 x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
            const f32 y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
            const f32 z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
            const f32 w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
            out[0] = x; out[1] = y; out[2] = z; out[3] = w;
        }

        // v turned by the unit quaternion q
        void rotateVector(const f32 q[4], const f32 v[3], f32 out[3]) {
            // t = 2 * cross(q.xyz, v); out = v + w * t + cross(q.xyz, t)
            const f32 t[3] = {
                2.0f * (q[1] * v[2] - q[2] * v[1]),
                2.0f * (q[2] * v[0] - q[0] * v[2]),
                2.0f * (q[0] * v[1] - q[1] * v[0]),
            };
            out[0] = v[0] + q[3] * t[0] + (q[1] * t[2] - q[2] * t[1]);
            out[1] = v[1] + q[3] * t[1] + (q[2] * t[0] - q[0] * t[2]);
            out[2] = v[2] + q[3] * t[2] + (q[0] * t[1] - q[1] * t[0]);
        }
    }

    NodeTransform nodeTransform(const Node& node) {
        NodeTransform result;
        std::copy(std::begin(node.translation), std::end(node.translation), result.translation);
        std::copy(std::begin(node.rotation), std::end(node.rotation), result.rotation);
        std::copy(std::begin(node.scale), std::end(node.scale), result.scale);
        return result;
    }

    NodeTransform combine(const NodeTransform& parent, const NodeTransform& local) {
        NodeTransform world;

        const f32 scaled[3] = { parent.scale[0] * local.translation[0], parent.scale[1] * local.translation[1], parent.scale[2] * local.translation[2] };
        f32 turned[3];
        rotateVector(parent.rotation, scaled, turned);
        for (int k = 0; k < 3; ++k) world.translation[k] = parent.translation[k] + turned[k];

        quaternionMultiply(parent.rotation, local.rotation, world.rotation);
        for (int k = 0; k < 3; ++k) world.scale[k] = parent.scale[k] * local.scale[k];
        return world;
    }

    // ---- File ----

    std::vector<NodeTransform> File::worldTransforms() const {
        std::vector<NodeTransform> world(m_nodes.size());
        for (std::size_t i = 0; i < m_nodes.size(); ++i) {
            const NodeTransform local = nodeTransform(m_nodes[i]);
            world[i] = m_nodes[i].parent == NONE ? local : combine(world[m_nodes[i].parent], local);
        }
        return world;
    }

    bool File::fail(const std::string& message) {
        m_error = message;
        m_data = nullptr;
        return false;
    }

    const DirectoryEntry* File::find(u32 type) const {
        for (const DirectoryEntry& entry : m_sections) {
            if (entry.type == type) return &entry;
        }
        return nullptr;
    }

    std::string_view File::string(StringRef ref) const {
        if (!m_strings || !fits(ref.offset, ref.length, m_stringBytes)) return {};
        return { reinterpret_cast<const char*>(m_strings + ref.offset), ref.length };
    }

    bool File::open(const void* data, std::size_t size, const ReadOptions& options) {
        *this = File();
        m_data = static_cast<const u8*>(data);
        m_size = size;

        // 1. Header
        if (reinterpret_cast<std::uintptr_t>(data) % 8 != 0) return fail("the buffer must start on an 8-byte boundary");
        if (size < HEADER_SIZE) return fail("too small to be a .vlmobj file");

        const Header& head = header();
        if (std::memcmp(head.magic, MAGIC, 4) != 0) return fail("not a .vlmobj file");
        if (head.version == 0 || head.version > VERSION) return fail("format version " + std::to_string(head.version) + " is newer than this reader (" + std::to_string(VERSION) + ")");
        if (crc32(m_data, offsetof(Header, headerCrc)) != head.headerCrc) return fail("the header is damaged");
        if (head.fileSize != size) return fail("the file is " + std::to_string(size) + " bytes but should be " + std::to_string(head.fileSize) + " (cut short?)");
        if (head.headerSize < HEADER_SIZE || head.headerSize > size) return fail("the header size is out of range");

        // 2. Directory
        if (head.directoryOffset % 8 != 0 || !fits(head.directoryOffset, u64(head.sectionCount) * sizeof(DirectoryEntry), size)) {
            return fail("the section list runs past the end of the file");
        }
        const u8* directory = m_data + head.directoryOffset;
        if (crc32(directory, std::size_t(head.sectionCount) * sizeof(DirectoryEntry)) != head.directoryCrc) return fail("the section list is damaged");
        m_sections = { reinterpret_cast<const DirectoryEntry*>(directory), head.sectionCount };

        // 3. Sections
        for (const DirectoryEntry& entry : m_sections) {
            const std::string name = typeName(entry.type);
            if (!fits(entry.offset, entry.storedSize, size)) return fail("section " + name + " runs past the end of the file");

            const u32 supported = supportedVersion(entry.type);
            if (supported == 0) continue;
            if (entry.version == 0 || entry.version > supported) return fail("section " + name + " is version " + std::to_string(entry.version) + ", newer than this reader");
            if (entry.compression != static_cast<u8>(Compression::None) || entry.size != entry.storedSize) return fail("section " + name + " uses a compression this reader doesn't know");
            if (entry.offset % 8 != 0) return fail("section " + name + " isn't aligned");
            if (options.verifyChecksums && crc32(m_data + entry.offset, entry.storedSize) != entry.crc) return fail("section " + name + " is damaged (checksum mismatch)");
        }

        auto records = [&](u32 type, std::size_t recordSize, const u8*& out, u64& count) {
            const DirectoryEntry* entry = find(type);
            if (!entry) return true;
            if (entry->size != u64(entry->count) * recordSize) return false;
            out = m_data + entry->offset;
            count = entry->count;
            return true;
        };

        const u8* nodes = nullptr;
        const u8* meshes = nullptr;
        const u8* parts = nullptr;
        const u8* materials = nullptr;
        u64 nodeCount = 0, meshCount = 0, partCount = 0, materialCount = 0;
        if (!records(Section::NODES, sizeof(Node), nodes, nodeCount)) return fail("section NODE has the wrong size for its count");
        if (!records(Section::MESHES, sizeof(Mesh), meshes, meshCount)) return fail("section MESH has the wrong size for its count");
        if (!records(Section::PARTS, sizeof(Part), parts, partCount)) return fail("section PART has the wrong size for its count");
        if (!records(Section::MATERIALS, sizeof(Material), materials, materialCount)) return fail("section MATL has the wrong size for its count");

        m_nodes = { reinterpret_cast<const Node*>(nodes), static_cast<std::size_t>(nodeCount) };
        m_meshes = { reinterpret_cast<const Mesh*>(meshes), static_cast<std::size_t>(meshCount) };
        m_parts = { reinterpret_cast<const Part*>(parts), static_cast<std::size_t>(partCount) };
        m_materials = { reinterpret_cast<const Material*>(materials), static_cast<std::size_t>(materialCount) };
        if (const DirectoryEntry* entry = find(Section::MATERIALS)) m_materialVersion = entry->version;

        // Textures: the records, then their data in the same section
        if (const DirectoryEntry* entry = find(Section::TEXTURES)) {
            if (entry->size < u64(entry->count) * sizeof(Texture)) return fail("section TEXR is too small for its count");
            m_textureSection = m_data + entry->offset;
            m_textures = { reinterpret_cast<const Texture*>(m_textureSection), static_cast<std::size_t>(entry->count) };
            for (std::size_t i = 0; i < m_textures.size(); ++i) {
                const Texture& texture = m_textures[i];
                const std::string which = "texture " + std::to_string(i);
                if (texture.format != static_cast<u8>(TextureFormat::Png)) return fail(which + " has a format this reader doesn't know");
                if (texture.width == 0 || texture.height == 0) return fail(which + " has no size");
                if (texture.dataOffset < u64(entry->count) * sizeof(Texture) || !fits(texture.dataOffset, texture.dataSize, entry->size)) return fail(which + "'s data is out of range");
            }
        }

        const DirectoryEntry* strings = find(Section::STRINGS);
        if (!strings || strings->size == 0 || m_data[strings->offset] != 0) return fail("the string table is missing or doesn't start with an empty string");
        m_strings = m_data + strings->offset;
        m_stringBytes = strings->size;

        if (const DirectoryEntry* vertices = find(Section::VERTICES)) {
            m_vertices = m_data + vertices->offset;
            m_vertexBytes = vertices->size;
        }
        if (const DirectoryEntry* indices = find(Section::INDICES)) {
            m_indices = m_data + indices->offset;
            m_indexBytes = indices->size;
        }

        // 4. Nodes: the root first, parents before children, links in range
        if (m_nodes.empty()) return fail("the asset has no nodes");
        for (std::size_t i = 0; i < m_nodes.size(); ++i) {
            const Node& node = m_nodes[i];
            if (i == 0 ? node.parent != NONE : node.parent >= i) return fail("node " + std::to_string(i) + " has a parent that isn't before it (the root must be first)");
            if (node.mesh != NONE && node.mesh >= m_meshes.size()) return fail("node " + std::to_string(i) + " points at a mesh that doesn't exist");
            if (!fits(node.name.offset, node.name.length, m_stringBytes)) return fail("node " + std::to_string(i) + "'s name is out of range");
        }

        // 5. Materials
        for (std::size_t i = 0; i < m_materials.size(); ++i) {
            const Material& material = m_materials[i];
            if (!fits(material.name.offset, material.name.length, m_stringBytes)) return fail("material " + std::to_string(i) + "'s name is out of range");
            if (material.alphaMode > static_cast<u8>(AlphaMode::Blend)) return fail("material " + std::to_string(i) + " has an alpha mode this reader doesn't know");
            const u32 texture = baseColorTexture(material);
            if (texture != NONE && texture >= m_textures.size()) return fail("material " + std::to_string(i) + " points at a texture that doesn't exist");
        }

        if (!checkMeshes(options)) return false;
        return true;
    }

    bool File::checkMeshes(const ReadOptions& options) {
        for (std::size_t i = 0; i < m_meshes.size(); ++i) {
            const Mesh& mesh = m_meshes[i];
            const std::string which = "mesh " + std::to_string(i);

            if (!fits(mesh.name.offset, mesh.name.length, m_stringBytes)) return fail(which + "'s name is out of range");
            if (mesh.primitive != 0) return fail(which + " uses a primitive type this reader doesn't know");
            if (mesh.indexSize != 2 && mesh.indexSize != 4) return fail(which + " has an index size other than 2 or 4");
            if (mesh.indexCount % 3 != 0) return fail(which + "'s index count isn't a multiple of 3");
            if (mesh.attributeCount > MAX_ATTRIBUTES) return fail(which + " has too many vertex attributes");
            if (mesh.vertexOffset % DATA_ALIGNMENT != 0 || mesh.indexOffset % DATA_ALIGNMENT != 0) return fail(which + "'s data isn't 16-byte aligned");

            for (u32 a = 0; a < mesh.attributeCount; ++a) {
                const VertexAttribute& attribute = mesh.attributes[a];
                const u32 bytes = formatSize(static_cast<Format>(attribute.format));
                if (attribute.semantic == 0 || attribute.semantic > static_cast<u8>(Semantic::Weights) || bytes == 0) return fail(which + " has a vertex attribute this reader doesn't know");
                if (u64(attribute.offset) + bytes > mesh.vertexStride) return fail(which + " has a vertex attribute outside its vertex");
            }

            if (!fits(mesh.vertexOffset, u64(mesh.vertexCount) * mesh.vertexStride, m_vertexBytes)) return fail(which + "'s vertices run past the vertex data");
            if (!fits(mesh.indexOffset, u64(mesh.indexCount) * mesh.indexSize, m_indexBytes)) return fail(which + "'s indices run past the index data");
            if (!fits(mesh.firstPart, mesh.partCount, m_parts.size())) return fail(which + "'s parts run past the part list");

            for (u32 p = 0; p < mesh.partCount; ++p) {
                const Part& part = m_parts[mesh.firstPart + p];
                if (!fits(part.firstIndex, part.indexCount, mesh.indexCount) || part.indexCount % 3 != 0) return fail(which + " has a part outside its indices");
                if (part.material != NONE && part.material >= m_materials.size()) return fail(which + " has a part whose material doesn't exist");
            }

            if (!options.verifyIndices) continue;

            const u8* indices = indexData(mesh);
            for (u32 k = 0; k < mesh.indexCount; ++k) {
                u32 index = 0;
                if (mesh.indexSize == 2) {
                    u16 small = 0;
                    std::memcpy(&small, indices + std::size_t(k) * 2, 2);
                    index = small;
                } else {
                    std::memcpy(&index, indices + std::size_t(k) * 4, 4);
                }
                if (index >= mesh.vertexCount) return fail(which + " has an index past its last vertex");
            }
        }

        return true;
    }

    bool File::readEditData(EditData& out) const {
        const DirectoryEntry* entry = find(Section::EDIT);
        if (!entry || !m_data) return false;

        Cursor cursor { m_data + entry->offset, static_cast<std::size_t>(entry->size) };
        EditData edit;

        u32 nodeCount = 0;
        if (!cursor.read(nodeCount) || nodeCount != m_nodes.size()) return false;
        if (!cursor.readArray(edit.eulerRotations, u64(nodeCount) * 3)) return false;

        u32 meshCount = 0;
        if (!cursor.read(meshCount) || meshCount != m_meshes.size()) return false;
        edit.meshes.resize(meshCount);

        for (EditMesh& mesh : edit.meshes) {
            u32 vertexCount = 0, faceCount = 0, cornerCount = 0;
            if (!cursor.read(vertexCount) || !cursor.read(faceCount) || !cursor.read(cornerCount)) return false;
            if (!cursor.readArray(mesh.positions, u64(vertexCount) * 3)) return false;
            if (!cursor.readArray(mesh.faceSizes, faceCount)) return false;
            if (!cursor.readArray(mesh.corners, cornerCount)) return false;

            u64 total = 0;
            for (u32 sides : mesh.faceSizes) {
                if (sides < 3) return false;
                total += sides;
            }
            if (total != cornerCount) return false;
            for (u32 corner : mesh.corners) if (corner >= vertexCount) return false;

            // Version 1 meshes carry no materials
            if (entry->version < 2) continue;
            u32 faceMaterialCount = 0;
            if (!cursor.read(mesh.material) || !cursor.read(faceMaterialCount)) return false;
            if (faceMaterialCount != 0 && faceMaterialCount != faceCount) return false;
            if (!cursor.readArray(mesh.faceMaterials, faceMaterialCount)) return false;

            const auto known = [&](u32 material) { return material == NONE || material < m_materials.size(); };
            if (!known(mesh.material)) return false;
            for (u32 material : mesh.faceMaterials) if (!known(material)) return false;

            // Version 2 meshes carry no UVs
            if (entry->version < 3) continue;
            u32 uvCount = 0;
            if (!cursor.read(uvCount)) return false;
            if (uvCount != 0 && uvCount != cornerCount) return false;
            if (!cursor.readArray(mesh.uvs, u64(uvCount) * 2)) return false;

            // Version 3 meshes are flat with no marks
            if (entry->version < 4) continue;
            u32 markCount = 0;
            if (!cursor.read(mesh.shading) || !cursor.read(mesh.smoothAngle) || !cursor.read(markCount)) return false;
            if (mesh.shading > 2 || !(mesh.smoothAngle >= 0.0f && mesh.smoothAngle <= 3.1416f)) return false;
            if (markCount != 0 && markCount != cornerCount) return false;
            if (!cursor.readArray(mesh.edgeMarks, markCount)) return false;
            for (u8 mark : mesh.edgeMarks) if (mark > 2) return false;
        }

        out = std::move(edit);
        return true;
    }

    // ---- Writer ----

    u32 Writer::addNode(const NodeInput& node) {
        m_nodes.push_back(node);
        return static_cast<u32>(m_nodes.size() - 1);
    }

    u32 Writer::addTexture(TextureInput texture) {
        m_textures.push_back(std::move(texture));
        return static_cast<u32>(m_textures.size() - 1);
    }

    u32 Writer::addMaterial(const MaterialInput& material) {
        m_materials.push_back(material);
        return static_cast<u32>(m_materials.size() - 1);
    }

    u32 Writer::addMesh(MeshInput mesh) {
        m_meshes.push_back(std::move(mesh));
        return static_cast<u32>(m_meshes.size() - 1);
    }

    void Writer::setEditData(EditData edit) {
        m_edit = std::move(edit);
        m_hasEdit = true;
    }

    std::vector<u8> Writer::finish() const {
        // Strings: one zero byte first (the empty string), then each distinct name once
        std::vector<u8> strings { 0 };
        std::map<std::string, StringRef> interned;
        auto intern = [&](const std::string& text) -> StringRef {
            if (text.empty()) return { 0, 0 };
            auto found = interned.find(text);
            if (found != interned.end()) return found->second;

            const StringRef ref { static_cast<u32>(strings.size()), static_cast<u32>(text.size()) };
            strings.insert(strings.end(), text.begin(), text.end());
            strings.push_back(0);
            interned.emplace(text, ref);
            return ref;
        };

        std::vector<u8> nodes;
        for (const NodeInput& input : m_nodes) {
            Node node {};
            node.name = intern(input.name);
            node.parent = input.parent;
            node.mesh = input.mesh;
            std::copy(std::begin(input.translation), std::end(input.translation), node.translation);
            std::copy(std::begin(input.rotation), std::end(input.rotation), node.rotation);
            std::copy(std::begin(input.scale), std::end(input.scale), node.scale);
            append(nodes, node);
        }

        std::vector<u8> materials;
        for (const MaterialInput& input : m_materials) {
            Material material {};
            material.name = intern(input.name);
            std::copy(std::begin(input.baseColor), std::end(input.baseColor), material.baseColor);
            material.roughness = input.roughness;
            material.metallic = input.metallic;
            std::copy(std::begin(input.emissiveColor), std::end(input.emissiveColor), material.emissiveColor);
            material.emissiveStrength = input.emissiveStrength;
            material.opacity = input.opacity;
            material.alphaCutoff = input.alphaCutoff;
            material.alphaMode = static_cast<u8>(input.alphaMode);
            material.flags = input.doubleSided ? MATERIAL_DOUBLE_SIDED : 0;
            material.baseColorTexture = input.baseColorTexture;
            append(materials, material);
        }

        // Texture records first, then each texture's data on a 16-byte boundary
        std::vector<u8> textures(m_textures.size() * sizeof(Texture), 0);
        for (std::size_t i = 0; i < m_textures.size(); ++i) {
            const TextureInput& input = m_textures[i];
            textures.resize(aligned(textures.size(), DATA_ALIGNMENT), 0);

            Texture texture {};
            texture.name = intern(input.name);
            texture.width = input.width;
            texture.height = input.height;
            texture.format = static_cast<u8>(TextureFormat::Png);
            texture.dataOffset = textures.size();
            texture.dataSize = input.png.size();
            std::memcpy(textures.data() + i * sizeof(Texture), &texture, sizeof(Texture));
            textures.insert(textures.end(), input.png.begin(), input.png.end());
        }

        std::vector<u8> meshes, parts, vertices, indices;
        u32 partCount = 0;

        for (const MeshInput& input : m_meshes) {
            Mesh mesh {};
            mesh.name = intern(input.name);
            mesh.vertexStride = input.vertexStride;
            mesh.vertexCount = input.vertexStride == 0 ? 0 : static_cast<u32>(input.vertices.size() / input.vertexStride);
            mesh.indexCount = static_cast<u32>(input.indices.size());
            mesh.indexSize = mesh.vertexCount <= 65536 ? 2 : 4;
            mesh.attributeCount = static_cast<u8>(std::min<std::size_t>(input.attributes.size(), MAX_ATTRIBUTES));
            for (u32 a = 0; a < mesh.attributeCount; ++a) mesh.attributes[a] = input.attributes[a];

            // Vertex and index data each start on a 16-byte boundary
            vertices.resize(aligned(vertices.size(), DATA_ALIGNMENT), 0);
            mesh.vertexOffset = vertices.size();
            vertices.insert(vertices.end(), input.vertices.begin(), input.vertices.end());

            indices.resize(aligned(indices.size(), DATA_ALIGNMENT), 0);
            mesh.indexOffset = indices.size();
            if (mesh.indexSize == 2) {
                for (u32 index : input.indices) append(indices, static_cast<u16>(index));
            } else {
                appendArray(indices, input.indices);
            }

            mesh.firstPart = partCount;
            std::vector<PartInput> inputParts = input.parts;
            if (inputParts.empty()) inputParts.push_back({ 0, mesh.indexCount, NONE });
            for (const PartInput& p : inputParts) append(parts, Part { p.firstIndex, p.indexCount, p.material, 0 });
            mesh.partCount = static_cast<u32>(inputParts.size());
            partCount += mesh.partCount;

            // Bounds from the positions
            const VertexAttribute* position = findAttribute(input.attributes, Semantic::Position);
            if (position && position->format == static_cast<u8>(Format::F32x3) && mesh.vertexCount > 0) {
                f32 low[3] = { INFINITY, INFINITY, INFINITY };
                f32 high[3] = { -INFINITY, -INFINITY, -INFINITY };
                auto at = [&](u32 v, f32 out[3]) { std::memcpy(out, input.vertices.data() + std::size_t(v) * input.vertexStride + position->offset, 12); };

                for (u32 v = 0; v < mesh.vertexCount; ++v) {
                    f32 p[3];
                    at(v, p);
                    for (int k = 0; k < 3; ++k) {
                        low[k] = std::min(low[k], p[k]);
                        high[k] = std::max(high[k], p[k]);
                    }
                }

                f32 radiusSq = 0.0f;
                for (int k = 0; k < 3; ++k) {
                    mesh.boundsMin[k] = low[k];
                    mesh.boundsMax[k] = high[k];
                    mesh.sphereCenter[k] = (low[k] + high[k]) * 0.5f;
                }
                for (u32 v = 0; v < mesh.vertexCount; ++v) {
                    f32 p[3];
                    at(v, p);
                    f32 distanceSq = 0.0f;
                    for (int k = 0; k < 3; ++k) distanceSq += (p[k] - mesh.sphereCenter[k]) * (p[k] - mesh.sphereCenter[k]);
                    radiusSq = std::max(radiusSq, distanceSq);
                }
                mesh.sphereRadius = std::sqrt(radiusSq);
            }

            append(meshes, mesh);
        }

        struct Pending {
            u32 type;
            u32 count;
            u8 flags;
            const std::vector<u8>* bytes;
        };

        const std::vector<u8> edit = m_hasEdit ? encodeEdit(m_edit) : std::vector<u8>();

        std::vector<Pending> sections = {
            { Section::STRINGS, 0, 0, &strings },
            { Section::NODES, static_cast<u32>(m_nodes.size()), 0, &nodes },
        };
        if (!m_meshes.empty()) {
            sections.push_back({ Section::MESHES, static_cast<u32>(m_meshes.size()), 0, &meshes });
            sections.push_back({ Section::PARTS, partCount, 0, &parts });
            sections.push_back({ Section::VERTICES, 0, 0, &vertices });
            sections.push_back({ Section::INDICES, 0, 0, &indices });
        }
        if (!m_materials.empty()) sections.push_back({ Section::MATERIALS, static_cast<u32>(m_materials.size()), 0, &materials });
        if (!m_textures.empty()) sections.push_back({ Section::TEXTURES, static_cast<u32>(m_textures.size()), 0, &textures });
        if (m_hasEdit) sections.push_back({ Section::EDIT, 0, SECTION_EDITOR_ONLY, &edit });

        // Header, directory, then each section on a 64-byte boundary
        const u64 directoryOffset = HEADER_SIZE;
        u64 offset = directoryOffset + sections.size() * sizeof(DirectoryEntry);

        std::vector<DirectoryEntry> directory;
        for (const Pending& section : sections) {
            offset = aligned(offset, SECTION_ALIGNMENT);
            DirectoryEntry entry {};
            entry.type = section.type;
            entry.version = supportedVersion(section.type);
            entry.offset = offset;
            entry.storedSize = section.bytes->size();
            entry.size = section.bytes->size();
            entry.crc = crc32(section.bytes->data(), section.bytes->size());
            entry.compression = static_cast<u8>(Compression::None);
            entry.flags = section.flags;
            entry.count = section.count;
            directory.push_back(entry);
            offset += section.bytes->size();
        }

        std::vector<u8> file(static_cast<std::size_t>(offset), 0);
        std::memcpy(file.data() + directoryOffset, directory.data(), directory.size() * sizeof(DirectoryEntry));
        for (std::size_t i = 0; i < sections.size(); ++i) {
            if (!sections[i].bytes->empty()) std::memcpy(file.data() + directory[i].offset, sections[i].bytes->data(), sections[i].bytes->size());
        }

        Header head {};
        std::memcpy(head.magic, MAGIC, 4);
        head.version = VERSION;
        head.headerSize = HEADER_SIZE;
        head.sectionCount = static_cast<u32>(directory.size());
        head.directoryCrc = crc32(directory.data(), directory.size() * sizeof(DirectoryEntry));
        head.directoryOffset = directoryOffset;
        head.fileSize = file.size();
        head.metersPerUnit = 1.0f;
        head.upAxis = 1;
        head.handedness = 0;
        head.headerCrc = crc32(&head, offsetof(Header, headerCrc));
        std::memcpy(file.data(), &head, sizeof(head));

        return file;
    }
}
