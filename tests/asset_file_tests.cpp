#include "test.hpp"
#include "mesh_helpers.hpp"
#include "asset/asset_file.hpp"
#include "vlmobj/vlmobj.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace {
    Object objectWith(const std::string& name, PresetMesh preset) {
        Object object;
        object.name = name;
        object.meshData.setMesh(preset);
        return object;
    }

    bool near(f32 a, f32 b, f32 tolerance = 1e-5f) {
        return std::fabs(a - b) <= tolerance;
    }

    // The same rotation: both turn a few test directions the same way
    bool sameRotation(const Vec3& a, const Vec3& b) {
        Transform ta, tb;
        ta.rotation = a;
        tb.rotation = b;
        const ObjectSpace sa(ta), sb(tb);
        for (const Vec3& point : { Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0.3f, -0.5f, 0.8f) }) {
            const Vec3 pa = sa.pointToWorld(point), pb = sb.pointToWorld(point);
            if (!near(pa.x, pb.x, 1e-4f) || !near(pa.y, pb.y, 1e-4f) || !near(pa.z, pb.z, 1e-4f)) return false;
        }
        return true;
    }

    std::filesystem::path referenceCube() {
        return std::filesystem::path(SOURCE_DIR) / "shared" / "vlmobj" / "reference" / "cube.vlmobj";
    }

    Object referenceObject() {
        return objectWith("Cube", PresetMesh::Cube);
    }

    // Writes the export without its EDIT section, as a file from another tool would be
    std::vector<u8> withoutEdit(const std::vector<u8>& bytes) {
        vlmobj::File file;
        file.open(bytes.data(), bytes.size());

        vlmobj::Writer writer;
        const vlmobj::Mesh& mesh = file.meshes()[0];
        vlmobj::MeshInput input;
        input.vertexStride = mesh.vertexStride;
        input.attributes.assign(mesh.attributes, mesh.attributes + mesh.attributeCount);
        input.vertices.assign(file.vertexData(mesh), file.vertexData(mesh) + std::size_t(mesh.vertexCount) * mesh.vertexStride);
        for (u32 k = 0; k < mesh.indexCount; ++k) {
            u16 index = 0;
            std::memcpy(&index, file.indexData(mesh) + std::size_t(k) * 2, 2);
            input.indices.push_back(index);
        }

        vlmobj::NodeInput node;
        node.name = std::string(file.string(file.nodes()[0].name));
        std::memcpy(node.rotation, file.nodes()[0].rotation, sizeof(node.rotation));
        std::memcpy(node.scale, file.nodes()[0].scale, sizeof(node.scale));
        node.mesh = writer.addMesh(input);
        writer.addNode(node);
        return writer.finish();
    }
}

TEST_CASE(asset_bake_shares_vertices_on_flat_faces) {
    // Cube: 4 per side (sides face different ways), 2 triangles per side
    const AssetFile::BakedMesh cubeMesh = AssetFile::bake(cube());
    CHECK(cubeMesh.vertices.size() / 6 == 24);
    CHECK(cubeMesh.indices.size() == 36);

    // A flat grid shares across every face: 11 × 11 corners
    MeshData grid;
    grid.setMesh(PresetMesh::Grid);
    const AssetFile::BakedMesh gridMesh = AssetFile::bake(grid);
    CHECK(gridMesh.vertices.size() / 6 == 121);
    CHECK(gridMesh.indices.size() == 600);

    // Every normal is unit length and every index in range
    bool valid = true;
    for (std::size_t v = 0; v < cubeMesh.vertices.size(); v += 6) {
        const Vec3 normal(cubeMesh.vertices[v + 3], cubeMesh.vertices[v + 4], cubeMesh.vertices[v + 5]);
        valid = valid && near(normal.length(), 1.0f);
    }
    for (u32 index : cubeMesh.indices) valid = valid && index < cubeMesh.vertices.size() / 6;
    CHECK(valid);
}

TEST_CASE(asset_bake_keeps_bent_faces_folded) {
    // Pull one corner of a cube outward: its faces aren't flat any more, so their triangles keep their own normals
    MeshData mesh = cube();
    VertexHandle top = INVALID_VERTEX;
    for (VertexHandle vertex : mesh.getVertexHandles()) {
        const Vec3 p = mesh.getVertexPosition(vertex);
        if (p.x > 0 && p.y > 0 && p.z > 0) top = vertex;
    }
    mesh.translateVertex(top, Vec3(0.2f, 0.2f, 0.2f));

    // Pulled out diagonally, the corner bends all three faces around it; each now has two triangle normals, 6 corners instead of 4
    const AssetFile::BakedMesh baked = AssetFile::bake(mesh);
    CHECK(baked.vertices.size() / 6 == 24 + 3 * 2);
    CHECK(baked.indices.size() == 36);
}

TEST_CASE(asset_quaternions_match_euler_rotations) {
    for (const Vec3& euler : { Vec3(0.0f), Vec3(0.3f, 0.0f, 0.0f), Vec3(0.2f, -1.1f, 2.5f), Vec3(1.0f, 1.5707964f, 0.4f), Vec3(-2.0f, 0.4f, -0.7f) }) {
        f32 q[4];
        AssetFile::eulerToQuaternion(euler, q);
        CHECK(near(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3], 1.0f));

        // Turning back gives the same rotation (not necessarily the same angles)
        CHECK(sameRotation(AssetFile::quaternionToEuler(q), euler));
    }
}

TEST_CASE(asset_round_trip_keeps_polygons_and_transform) {
    Object object = objectWith("Crate", PresetMesh::Cube);
    SlideSession session;
    object.meshData.bevelVertex(object.meshData.getVertexHandles()[0], session);
    object.meshData.setSlideWidth(session, 0.25f);
    object.meshData.removeFace(object.meshData.getFaceHandles()[1]);
    object.transform.position = Vec3(4.0f, 5.0f, 6.0f);
    object.transform.rotation = Vec3(0.1f, 0.2f, 0.3f);
    object.transform.scale = Vec3(1.0f, 2.0f, 3.0f);

    Object loaded;
    std::string error;
    CHECK(AssetFile::read(AssetFile::write(object), loaded, error));
    CHECK(error.empty());

    // Origin as pivot: the position is dropped, rotation (exact) and scale kept
    CHECK(loaded.name == "Crate");
    CHECK(loaded.transform.position.x == 0.0f && loaded.transform.position.y == 0.0f);
    CHECK(loaded.transform.rotation.x == 0.1f && loaded.transform.rotation.z == 0.3f);
    CHECK(loaded.transform.scale.y == 2.0f && loaded.transform.scale.z == 3.0f);

    // The same polygons, n-gons and the open side included
    CHECK(loaded.meshData.validate());
    CHECK(faceSides(loaded.meshData) == faceSides(object.meshData));
    CHECK(counts(loaded.meshData, object.meshData.getVertexHandles().size(), object.meshData.getEdgeHandles().size(), object.meshData.getFaceHandles().size()));

    bool samePositions = true;
    const std::vector<VertexHandle> a = object.meshData.getVertexHandles();
    const std::vector<VertexHandle> b = loaded.meshData.getVertexHandles();
    for (std::size_t i = 0; i < a.size(); ++i) {
        const Vec3 pa = object.meshData.getVertexPosition(a[i]), pb = loaded.meshData.getVertexPosition(b[i]);
        samePositions = samePositions && pa.x == pb.x && pa.y == pb.y && pa.z == pb.z;
    }
    CHECK(samePositions);
}

TEST_CASE(asset_without_polygons_is_rebuilt_from_triangles) {
    Object object = objectWith("Box", PresetMesh::Cube);
    object.transform.rotation = Vec3(0.4f, -0.2f, 1.0f);
    object.transform.scale = Vec3(2.0f, 1.0f, 1.0f);

    Object loaded;
    std::string error;
    CHECK(AssetFile::read(withoutEdit(AssetFile::write(object)), loaded, error));

    // Corners at the same position join, so the cube is closed again: 8 vertices, 12 triangles
    CHECK(loaded.meshData.validate());
    CHECK(counts(loaded.meshData, 8, 36, 12));
    CHECK(loaded.name == "Box");
    CHECK(sameRotation(loaded.transform.rotation, object.transform.rotation));
    CHECK(loaded.transform.scale.x == 2.0f);
}

TEST_CASE(asset_read_refuses_bad_files) {
    Object loaded;
    std::string error;

    std::vector<u8> bytes = AssetFile::write(referenceObject());
    bytes[0] = 'X';
    CHECK(!AssetFile::read(bytes, loaded, error));
    CHECK(error.find("not a .vlmobj file") != std::string::npos);

    // Editable polygons that two faces both wind the same way along an edge can't become a half-edge mesh
    vlmobj::Writer writer;
    vlmobj::MeshInput mesh;
    mesh.vertexStride = 12;
    mesh.attributes = { { static_cast<u8>(vlmobj::Semantic::Position), static_cast<u8>(vlmobj::Format::F32x3), 0, 0 } };
    mesh.vertices.assign(4 * 12, 0);
    mesh.indices = { 0, 1, 2 };
    vlmobj::NodeInput node;
    node.mesh = writer.addMesh(mesh);
    writer.addNode(node);
    vlmobj::EditData edit;
    edit.eulerRotations = { 0, 0, 0 };
    edit.meshes.push_back({ { 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0 }, { 3, 3 }, { 0, 1, 2, 0, 1, 3 } });
    writer.setEditData(edit);

    CHECK(!AssetFile::read(writer.finish(), loaded, error));
    CHECK(error.find("isn't a surface") != std::string::npos);
}

TEST_CASE(asset_save_and_load_through_a_file) {
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "valuma_asset_tests";
    std::filesystem::create_directories(folder);
    const std::filesystem::path path = folder / "Tree trunk.vlmobj";

    std::string error;
    CHECK(AssetFile::save(path, objectWith("Tree trunk", PresetMesh::Cylinder), error));
    CHECK(AssetFile::save(path, objectWith("Tree trunk", PresetMesh::Cylinder), error));
    std::filesystem::path leftover = path;
    leftover += ".exporting";
    CHECK(!std::filesystem::exists(leftover));

    Object loaded;
    CHECK(AssetFile::load(path, loaded, error));
    CHECK(loaded.name == "Tree trunk" && loaded.meshData.validate());

    CHECK(!AssetFile::load(folder / "missing.vlmobj", loaded, error));
    std::filesystem::remove_all(folder);
}

TEST_CASE(asset_export_matches_the_reference_cube) {
    // The checked-in reference must stay byte-identical to what Valuma exports; a difference means the format
    // changed. Run with VLMOBJ_WRITE_REFERENCE=1 to rewrite it after a deliberate change (and bump the versions).
    const std::vector<u8> bytes = AssetFile::write(referenceObject());

    if (std::getenv("VLMOBJ_WRITE_REFERENCE")) {
        std::filesystem::create_directories(referenceCube().parent_path());
        std::ofstream(referenceCube(), std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::vector<u8> reference;
    if (std::filesystem::exists(referenceCube())) {
        reference.resize(static_cast<std::size_t>(std::filesystem::file_size(referenceCube())));
        std::ifstream(referenceCube(), std::ios::binary).read(reinterpret_cast<char*>(reference.data()), static_cast<std::streamsize>(reference.size()));
    }
    CHECK(reference == bytes);
}

TEST_CASE(asset_family_round_trips_with_its_parents) {
    ObjectCollection objects;
    const ObjectHandle car = objects.add("Car", PresetMesh::Cube);
    const ObjectHandle wheel = objects.add("Wheel", PresetMesh::Cylinder);
    const ObjectHandle hubcap = objects.add("Hubcap", PresetMesh::Circle);
    const ObjectHandle other = objects.add("Other", PresetMesh::Torus);
    objects.get(car).transform.position = Vec3(10.0f, 0.0f, 0.0f);
    objects.get(car).transform.rotation = Vec3(0.0f, 0.5f, 0.0f);
    objects.get(car).transform.scale = Vec3(2.0f, 1.0f, 1.0f);
    objects.get(wheel).transform.position = Vec3(1.0f, -0.5f, 0.6f);
    objects.get(wheel).transform.rotation = Vec3(1.5707964f, 0.0f, 0.0f);
    objects.get(hubcap).transform.position = Vec3(0.0f, 0.2f, 0.0f);
    objects.get(wheel).parent = car;
    objects.get(hubcap).parent = wheel;
    (void)other;

    const std::vector<u8> bytes = AssetFile::write(objects, car);

    // The engine's view: three nodes, parents first, and world transforms by the shared rule
    vlmobj::File file;
    CHECK(file.open(bytes.data(), bytes.size()));
    CHECK(file.nodes().size() == 3);
    CHECK(file.nodes()[1].parent == 0 && file.nodes()[2].parent == 1);
    const std::vector<vlmobj::NodeTransform> world = file.worldTransforms();

    // Relative to the car's origin (the pivot), each part sits where it does in Valuma
    const Transform pivot = objects.worldTransform(car);
    const Transform hubWorld = objects.worldTransform(hubcap);
    const Vec3 expected = hubWorld.position - pivot.position;
    CHECK(near(world[2].translation[0], expected.x, 1e-4f) && near(world[2].translation[1], expected.y, 1e-4f) && near(world[2].translation[2], expected.z, 1e-4f));
    CHECK(near(world[2].scale[0], hubWorld.scale.x) && near(world[2].scale[1], hubWorld.scale.y));

    // Back in Valuma: the same family, relative transforms exactly as they were
    std::vector<AssetFile::ImportedObject> imported;
    std::string error;
    CHECK(AssetFile::read(bytes, imported, error));
    CHECK(imported.size() == 3);
    CHECK(imported[0].object.name == "Car" && imported[1].object.name == "Wheel" && imported[2].object.name == "Hubcap");
    CHECK(imported[1].parent == 0 && imported[2].parent == 1);
    CHECK(imported[1].object.transform.position.x == 1.0f && imported[1].object.transform.rotation.x == 1.5707964f);
    CHECK(imported[0].object.transform.scale.x == 2.0f && imported[0].object.transform.position.x == 0.0f);
    CHECK(imported[2].object.meshData.validate());
}
