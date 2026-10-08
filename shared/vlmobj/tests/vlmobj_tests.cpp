#include "test.hpp"
#include "vlmobj/vlmobj.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>

// Only the shared library is used here, as the engine would use it
namespace {
    using namespace vlmobj;

    // A unit quad in the XZ plane facing up: 4 vertices (position, normal), 2 triangles
    MeshInput quad() {
        MeshInput mesh;
        mesh.name = "Quad";
        mesh.vertexStride = 24;
        mesh.attributes = {
            { static_cast<u8>(Semantic::Position), static_cast<u8>(Format::F32x3), 0, 0 },
            { static_cast<u8>(Semantic::Normal), static_cast<u8>(Format::F32x3), 0, 12 },
        };
        const f32 vertices[] = {
            -0.5f, 0.0f, -0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.0f,  0.5f, 0.0f, 1.0f, 0.0f,
             0.5f, 0.0f,  0.5f, 0.0f, 1.0f, 0.0f,
             0.5f, 2.0f, -0.5f, 0.0f, 1.0f, 0.0f,
        };
        mesh.vertices.resize(sizeof(vertices));
        std::memcpy(mesh.vertices.data(), vertices, sizeof(vertices));
        mesh.indices = { 0, 1, 2, 0, 2, 3 };
        return mesh;
    }

    std::vector<u8> quadFile(bool withEdit = true) {
        Writer writer;
        NodeInput node;
        node.name = "Quad";
        node.mesh = writer.addMesh(quad());
        node.rotation[1] = 0.7071068f;
        node.rotation[3] = 0.7071068f;
        node.scale[0] = 2.0f;
        writer.addNode(node);

        if (withEdit) {
            EditData edit;
            edit.eulerRotations = { 0.0f, 1.5707964f, 0.0f };
            edit.meshes.push_back({ { -0.5f, 0, -0.5f, -0.5f, 0, 0.5f, 0.5f, 0, 0.5f, 0.5f, 2, -0.5f }, { 4 }, { 0, 1, 2, 3 } });
            writer.setEditData(edit);
        }
        return writer.finish();
    }

    template <typename T>
    T get(const std::vector<u8>& bytes, std::size_t offset) {
        T value {};
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }

    template <typename T>
    void put(std::vector<u8>& bytes, std::size_t offset, T value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(T));
    }

    std::size_t entryAt(u32 i) { return 64 + std::size_t(i) * sizeof(DirectoryEntry); }

    // After changing the directory: fix its checksum and the header's
    void reseal(std::vector<u8>& bytes) {
        const u32 count = get<u32>(bytes, 16);
        put(bytes, 20, crc32(bytes.data() + 64, std::size_t(count) * sizeof(DirectoryEntry)));
        put(bytes, 48, crc32(bytes.data(), 48));
    }

    // After changing a section's bytes: fix its checksum, then the directory's
    void resealSection(std::vector<u8>& bytes, u32 index) {
        const DirectoryEntry entry = get<DirectoryEntry>(bytes, entryAt(index));
        put(bytes, entryAt(index) + offsetof(DirectoryEntry, crc), crc32(bytes.data() + entry.offset, entry.size));
        reseal(bytes);
    }

    u32 sectionIndex(const std::vector<u8>& bytes, u32 type) {
        const u32 count = get<u32>(bytes, 16);
        for (u32 i = 0; i < count; ++i) if (get<DirectoryEntry>(bytes, entryAt(i)).type == type) return i;
        return NONE;
    }

    bool fails(const std::vector<u8>& bytes, const char* expected, ReadOptions options = {}) {
        File file;
        return !file.open(bytes.data(), bytes.size(), options) && std::string(file.error()).find(expected) != std::string::npos;
    }
}

TEST_CASE(vlmobj_crc32_check_value) {
    CHECK(crc32("123456789", 9) == 0xCBF43926u);
}

TEST_CASE(vlmobj_write_and_read_a_quad) {
    const std::vector<u8> bytes = quadFile();
    File file;
    CHECK(file.open(bytes.data(), bytes.size()));

    const Header& header = file.header();
    CHECK(header.version == VERSION && header.metersPerUnit == 1.0f && header.upAxis == 1 && header.handedness == 0);
    CHECK(header.fileSize == bytes.size());

    // Every section on a 64-byte boundary
    bool aligned = true;
    for (const DirectoryEntry& entry : file.sections()) aligned = aligned && entry.offset % 64 == 0;
    CHECK(aligned);

    CHECK(file.nodes().size() == 1);
    const Node& node = file.nodes()[0];
    CHECK(file.string(node.name) == "Quad");
    CHECK(node.parent == NONE && node.mesh == 0);
    CHECK(node.rotation[1] == 0.7071068f && node.scale[0] == 2.0f && node.scale[1] == 1.0f);

    CHECK(file.meshes().size() == 1);
    const Mesh& mesh = file.meshes()[0];
    CHECK(file.string(mesh.name) == "Quad");
    CHECK(mesh.vertexCount == 4 && mesh.indexCount == 6 && mesh.indexSize == 2 && mesh.vertexStride == 24);
    CHECK(mesh.attributeCount == 2 && mesh.attributes[1].semantic == static_cast<u8>(Semantic::Normal) && mesh.attributes[1].offset == 12);
    CHECK(mesh.boundsMin[0] == -0.5f && mesh.boundsMax[1] == 2.0f && mesh.boundsMin[1] == 0.0f);
    CHECK(mesh.sphereCenter[1] == 1.0f);
    CHECK(mesh.sphereRadius > 1.2f && mesh.sphereRadius < 1.3f);

    // One part covering everything, default material
    CHECK(mesh.partCount == 1 && file.parts()[mesh.firstPart].indexCount == 6 && file.parts()[mesh.firstPart].material == NONE);

    u16 indices[6];
    std::memcpy(indices, file.indexData(mesh), sizeof(indices));
    CHECK(indices[0] == 0 && indices[2] == 2 && indices[5] == 3);

    f32 last[6];
    std::memcpy(last, file.vertexData(mesh) + 3 * 24, sizeof(last));
    CHECK(last[1] == 2.0f && last[4] == 1.0f);

    // Editor-only section, with exact data
    const DirectoryEntry* edit = file.find(Section::EDIT);
    CHECK(edit && (edit->flags & SECTION_EDITOR_ONLY));
    EditData editData;
    CHECK(file.readEditData(editData));
    CHECK(editData.eulerRotations.size() == 3 && editData.eulerRotations[1] == 1.5707964f);
    CHECK(editData.meshes.size() == 1 && editData.meshes[0].faceSizes[0] == 4 && editData.meshes[0].corners[3] == 3);

    // Same input, same bytes
    CHECK(quadFile() == bytes);

    File noEdit;
    const std::vector<u8> plain = quadFile(false);
    CHECK(noEdit.open(plain.data(), plain.size()));
    CHECK(!noEdit.find(Section::EDIT));
    CHECK(!noEdit.readEditData(editData));
}

TEST_CASE(vlmobj_large_meshes_use_32_bit_indices) {
    MeshInput mesh = quad();
    const u32 vertexCount = 70000;
    mesh.vertices.assign(std::size_t(vertexCount) * 24, 0);
    mesh.indices.clear();
    for (u32 i = 0; i + 2 < vertexCount; i += 3) mesh.indices.insert(mesh.indices.end(), { i, i + 1, i + 2 });

    Writer writer;
    NodeInput node;
    node.mesh = writer.addMesh(mesh);
    writer.addNode(node);
    const std::vector<u8> bytes = writer.finish();

    File file;
    CHECK(file.open(bytes.data(), bytes.size()));
    CHECK(file.meshes()[0].indexSize == 4);

    u32 lastIndex = 0;
    std::memcpy(&lastIndex, file.indexData(file.meshes()[0]) + (std::size_t(file.meshes()[0].indexCount) - 1) * 4, 4);
    CHECK(lastIndex == mesh.indices.back());
}

TEST_CASE(vlmobj_refuses_damaged_files) {
    const std::vector<u8> bytes = quadFile();

    std::vector<u8> magic = bytes;
    magic[0] = 'X';
    CHECK(fails(magic, "not a .vlmobj file"));

    std::vector<u8> newer = bytes;
    put<u32>(newer, 4, VERSION + 1);
    put(newer, 48, crc32(newer.data(), 48));
    CHECK(fails(newer, "newer than this reader"));

    std::vector<u8> header = bytes;
    header[40] ^= 0x01;
    CHECK(fails(header, "header is damaged"));

    CHECK(fails(std::vector<u8>(bytes.begin(), bytes.end() - 8), "cut short"));
    CHECK(fails(std::vector<u8>(bytes.begin(), bytes.begin() + 20), "too small"));

    std::vector<u8> directory = bytes;
    directory[entryAt(0) + 4] ^= 0x01;
    CHECK(fails(directory, "section list is damaged"));

    // A flipped vertex byte: caught by the checksum, or passed when checks are off
    std::vector<u8> vertex = bytes;
    const DirectoryEntry vertices = get<DirectoryEntry>(bytes, entryAt(sectionIndex(bytes, Section::VERTICES)));
    vertex[vertices.offset + 4] ^= 0x01;
    CHECK(fails(vertex, "VTXS is damaged"));
    File unchecked;
    CHECK(unchecked.open(vertex.data(), vertex.size(), { .verifyChecksums = false }));

    // A section claiming to run past the end
    std::vector<u8> past = bytes;
    put<u64>(past, entryAt(0) + offsetof(DirectoryEntry, storedSize), past.size());
    reseal(past);
    CHECK(fails(past, "runs past the end"));

    // A section from a newer version
    std::vector<u8> newerSection = bytes;
    put<u32>(newerSection, entryAt(sectionIndex(bytes, Section::MESHES)) + offsetof(DirectoryEntry, version), 2);
    reseal(newerSection);
    CHECK(fails(newerSection, "MESH is version 2"));

    // Compressed sections aren't defined yet
    std::vector<u8> compressed = bytes;
    compressed[entryAt(sectionIndex(bytes, Section::NODES)) + offsetof(DirectoryEntry, compression)] = 1;
    reseal(compressed);
    CHECK(fails(compressed, "compression"));

    // The buffer must be 8-byte aligned so records can be read in place
    std::vector<u8> shifted(bytes.size() + 1);
    std::memcpy(shifted.data() + 1, bytes.data(), bytes.size());
    File misaligned;
    CHECK(!misaligned.open(shifted.data() + 1, bytes.size()));
}

TEST_CASE(vlmobj_refuses_links_out_of_range) {
    const std::vector<u8> bytes = quadFile();
    const u32 nodes = sectionIndex(bytes, Section::NODES);
    const u32 meshes = sectionIndex(bytes, Section::MESHES);
    const u32 indices = sectionIndex(bytes, Section::INDICES);
    const u64 nodeOffset = get<DirectoryEntry>(bytes, entryAt(nodes)).offset;
    const u64 meshOffset = get<DirectoryEntry>(bytes, entryAt(meshes)).offset;
    const u64 indexOffset = get<DirectoryEntry>(bytes, entryAt(indices)).offset;

    std::vector<u8> badMesh = bytes;
    put<u32>(badMesh, nodeOffset + offsetof(Node, mesh), 5);
    resealSection(badMesh, nodes);
    CHECK(fails(badMesh, "mesh that doesn't exist"));

    std::vector<u8> badParent = bytes;
    put<u32>(badParent, nodeOffset + offsetof(Node, parent), 0);
    resealSection(badParent, nodes);
    CHECK(fails(badParent, "root must be first"));

    std::vector<u8> badName = bytes;
    put<u32>(badName, nodeOffset + offsetof(Node, name), 9999);
    resealSection(badName, nodes);
    CHECK(fails(badName, "name is out of range"));

    std::vector<u8> badCount = bytes;
    put<u32>(badCount, meshOffset + offsetof(Mesh, vertexCount), 1000);
    resealSection(badCount, meshes);
    CHECK(fails(badCount, "vertices run past"));

    std::vector<u8> badFormat = bytes;
    badFormat[meshOffset + offsetof(Mesh, attributes) + 1] = 77;
    resealSection(badFormat, meshes);
    CHECK(fails(badFormat, "attribute this reader doesn't know"));

    // An index past the last vertex; the check can be turned off
    std::vector<u8> badIndex = bytes;
    put<u16>(badIndex, indexOffset + 2, 9);
    resealSection(badIndex, indices);
    CHECK(fails(badIndex, "index past its last vertex"));
    File unchecked;
    CHECK(unchecked.open(badIndex.data(), badIndex.size(), { .verifyIndices = false }));
}

TEST_CASE(vlmobj_skips_unknown_sections) {
    std::vector<u8> bytes = quadFile();

    // The EDIT section renamed to a type this reader doesn't know: skipped, even with a wrong checksum
    const u32 edit = sectionIndex(bytes, Section::EDIT);
    put<u32>(bytes, entryAt(edit), fourCC("ZZZZ"));
    put<u32>(bytes, entryAt(edit) + offsetof(DirectoryEntry, crc), 12345);
    reseal(bytes);

    File file;
    CHECK(file.open(bytes.data(), bytes.size()));
    CHECK(file.find(fourCC("ZZZZ")) != nullptr);
    CHECK(!file.find(Section::EDIT));
    CHECK(file.meshes().size() == 1);
}

TEST_CASE(vlmobj_reference_cube_reads_back) {
    // Written by Valuma's exporter and checked in (see asset_file_tests.cpp); any reader must keep reading it
    const std::filesystem::path path = std::filesystem::path(SOURCE_DIR) / "shared" / "vlmobj" / "reference" / "cube.vlmobj";
    std::vector<u8> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    std::ifstream(path, std::ios::binary).read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    File file;
    CHECK(file.open(bytes.data(), bytes.size()));
    CHECK(file.nodes().size() == 1 && file.string(file.nodes()[0].name) == "Cube");

    const Mesh& mesh = file.meshes()[0];
    CHECK(mesh.vertexCount == 24 && mesh.indexCount == 36 && mesh.indexSize == 2);
    CHECK(mesh.boundsMin[0] == -0.5f && mesh.boundsMax[2] == 0.5f);

    EditData edit;
    CHECK(file.readEditData(edit));
    CHECK(edit.meshes[0].positions.size() == 8 * 3 && edit.meshes[0].faceSizes.size() == 6);
}

TEST_CASE(vlmobj_combine_places_children_without_skew) {
    // Parent at (5, 0, 0), stretched 2x along X, turned 90 degrees about Y; a child one unit along its X
    NodeTransform parent;
    parent.translation[0] = 5.0f;
    parent.rotation[1] = 0.70710678f;
    parent.rotation[3] = 0.70710678f;
    parent.scale[0] = 2.0f;

    NodeTransform child;
    child.translation[0] = 1.0f;
    child.scale[1] = 3.0f;

    const NodeTransform world = combine(parent, child);
    auto near = [](f32 a, f32 b) { return a - b < 1e-5f && b - a < 1e-5f; };
    CHECK(near(world.translation[0], 5.0f) && near(world.translation[1], 0.0f) && near(world.translation[2], -2.0f));
    CHECK(near(world.rotation[1], 0.70710678f) && near(world.rotation[3], 0.70710678f));
    // Scales multiply along the child's own axes
    CHECK(near(world.scale[0], 2.0f) && near(world.scale[1], 3.0f) && near(world.scale[2], 1.0f));
}
