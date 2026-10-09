#include "test.hpp"
#include "mesh_helpers.hpp"
#include "asset/asset_file.hpp"
#include "project/project_file.hpp"
#include "scene/scene.hpp"
#include "vlmobj/vlmobj.hpp"

#include <set>

namespace {
    const MaterialHandle RED { 1, 0 };
    const MaterialHandle BLUE { 2, 0 };

    MeshData grid() {
        MeshData mesh;
        mesh.setMesh(PresetMesh::Grid);
        return mesh;
    }

    // Faces created by an operation: those not in before
    std::vector<FaceHandle> newFaces(const MeshData& mesh, const std::vector<FaceHandle>& before) {
        std::vector<FaceHandle> created;
        for (FaceHandle face : mesh.getFaceHandles()) {
            if (std::find(before.begin(), before.end(), face) == before.end()) created.push_back(face);
        }
        return created;
    }

    bool allHave(const MeshData& mesh, const std::vector<FaceHandle>& faces, MaterialHandle material) {
        for (FaceHandle face : faces) if (!(mesh.getFaceMaterial(face) == material)) return false;
        return !faces.empty();
    }

    // Two cells of the grid sharing an edge
    std::vector<FaceHandle> twoCells(const MeshData& mesh) {
        const FaceHandle first = mesh.getFaceHandles()[44];
        for (FaceHandle face : mesh.getFaceHandles()) {
            if (face == first) continue;
            u32 shared = 0;
            for (VertexHandle a : mesh.getFaceVertices(face)) {
                for (VertexHandle b : mesh.getFaceVertices(first)) {
                    if (a == b) ++shared;
                }
            }
            if (shared == 2) return { first, face };
        }
        return { first };
    }
}

TEST_CASE(extruding_faces_of_one_material_gives_the_sides_it) {
    MeshData mesh = grid();
    const std::vector<FaceHandle> region = twoCells(mesh);
    for (FaceHandle face : region) mesh.setFaceMaterial(face, RED);

    const std::vector<FaceHandle> before = mesh.getFaceHandles();
    std::vector<FaceHandle> tops;
    CHECK(mesh.extrudeRegions(region, tops) == RegionError::None);
    CHECK(allHave(mesh, tops, RED));
    CHECK(allHave(mesh, newFaces(mesh, before), RED));
}

TEST_CASE(extruding_faces_of_mixed_materials_gives_the_sides_the_objects) {
    MeshData mesh = grid();
    const std::vector<FaceHandle> region = twoCells(mesh);
    mesh.setFaceMaterial(region[0], RED);
    mesh.setFaceMaterial(region[1], BLUE);

    const std::vector<FaceHandle> before = mesh.getFaceHandles();
    std::vector<FaceHandle> tops;
    CHECK(mesh.extrudeRegions(region, tops) == RegionError::None);

    // The tops keep their own; the new sides use the object's
    CHECK(mesh.getFaceMaterial(tops[0]) == RED && mesh.getFaceMaterial(tops[1]) == BLUE);
    std::vector<FaceHandle> sides;
    for (FaceHandle face : newFaces(mesh, before)) {
        if (std::find(tops.begin(), tops.end(), face) == tops.end()) sides.push_back(face);
    }
    CHECK(!sides.empty());
    CHECK(allHave(mesh, sides, INVALID_MATERIAL));
}

TEST_CASE(inset_and_bevel_follow_the_same_rule) {
    MeshData mesh = grid();
    const FaceHandle face = mesh.getFaceHandles()[33];
    mesh.setFaceMaterial(face, BLUE);

    SlideSession session;
    std::vector<FaceHandle> inner;
    const std::vector<FaceHandle> before = mesh.getFaceHandles();
    CHECK(mesh.insetRegions({ face }, session, inner, Mat4()) == RegionError::None);
    CHECK(allHave(mesh, newFaces(mesh, before), BLUE));

    // A cube corner: three faces around it, two red, so the corner face gets the object's
    MeshData corner = cube();
    const VertexHandle vertex = corner.getVertexHandles()[0];
    std::vector<FaceHandle> around;
    for (FaceHandle candidate : corner.getFaceHandles()) {
        const std::vector<VertexHandle> loop = corner.getFaceVertices(candidate);
        if (std::find(loop.begin(), loop.end(), vertex) != loop.end()) around.push_back(candidate);
    }
    corner.setFaceMaterial(around[0], RED);
    corner.setFaceMaterial(around[1], RED);
    const std::vector<FaceHandle> cornerBefore = corner.getFaceHandles();
    SlideSession bevelSession;
    CHECK(corner.bevelVertex(vertex, bevelSession));
    // The reshaped faces come back with new handles; the new corner face is the triangle
    std::vector<FaceHandle> cornerFaces;
    for (FaceHandle created : newFaces(corner, cornerBefore)) {
        if (corner.getFaceVertices(created).size() == 3) cornerFaces.push_back(created);
    }
    CHECK(cornerFaces.size() == 1);
    CHECK(allHave(corner, cornerFaces, INVALID_MATERIAL));

    // The reshaped faces keep theirs
    u32 red = 0;
    for (FaceHandle candidate : corner.getFaceHandles()) if (corner.getFaceMaterial(candidate) == RED) ++red;
    CHECK(red == 2);
}

TEST_CASE(single_face_extrude_fill_and_connect_keep_materials) {
    MeshData mesh = cube();
    const FaceHandle top = mesh.getFaceHandles()[0];
    mesh.setFaceMaterial(top, RED);

    const std::vector<FaceHandle> before = mesh.getFaceHandles();
    extrude(mesh, top);
    CHECK(allHave(mesh, newFaces(mesh, before), RED));

    // Connecting two corners of a red face splits it into two red faces
    MeshData split = cube();
    const FaceHandle face = split.getFaceHandles()[1];
    split.setFaceMaterial(face, BLUE);
    const std::vector<VertexHandle> corners = split.getFaceVertices(face);
    CHECK(split.connectVertices(corners[0], corners[2]));
    u32 blue = 0;
    for (FaceHandle candidate : split.getFaceHandles()) if (split.getFaceMaterial(candidate) == BLUE) ++blue;
    CHECK(blue == 2);

    // Filling a hole takes the material its neighbors share
    MeshData open = cube();
    for (FaceHandle candidate : open.getFaceHandles()) open.setFaceMaterial(candidate, RED);
    const FaceHandle removed = open.getFaceHandles()[2];
    const EdgeHandle border = open.getFace(removed)->edge;
    CHECK(open.removeFace(removed));
    const std::vector<FaceHandle> beforeFill = open.getFaceHandles();
    CHECK(open.fillFaceLoop(border));
    CHECK(allHave(open, newFaces(open, beforeFill), RED));
}

TEST_CASE(face_data_groups_faces_by_material) {
    MeshData mesh = grid();
    const std::vector<FaceHandle> faces = mesh.getFaceHandles();
    for (std::size_t i = 0; i < faces.size(); i += 3) mesh.setFaceMaterial(faces[i], RED);
    for (std::size_t i = 1; i < faces.size(); i += 7) mesh.setFaceMaterial(faces[i], BLUE);

    const FaceData data = mesh.getFaceData();
    CHECK(data.groups.size() == 3);
    CHECK(data.groups[0].material.isNull());

    // Groups follow each other and cover every index; each face sits inside its own group's run
    u32 next = 0;
    for (const FaceGroup& group : data.groups) {
        CHECK(group.firstIndex == next);
        next += group.indexCount;
    }
    CHECK(next == data.indices.size());

    bool inside = true;
    for (FaceHandle face : faces) {
        const u32 first = data.indexMap[face.index * 2];
        const MaterialHandle material = mesh.getFaceMaterial(face);
        for (const FaceGroup& group : data.groups) {
            if (!(group.material == material)) continue;
            inside = inside && first >= group.firstIndex && first < group.firstIndex + group.indexCount;
        }
    }
    CHECK(inside);

    // A removed material groups with the object's
    const FaceData regrouped = mesh.getFaceData([](MaterialHandle material) { return material == BLUE ? INVALID_MATERIAL : material; });
    CHECK(regrouped.groups.size() == 2);
}

TEST_CASE(project_saves_face_materials) {
    Scene scene;
    const ObjectHandle handle = scene.objects.add("Crate", PresetMesh::Cube);
    Material wood;
    wood.name = "Wood";
    const MaterialHandle woodHandle = scene.materials.add(wood);
    Material metal;
    metal.name = "Metal";
    metal.metallic = 1.0f;
    const MaterialHandle metalHandle = scene.materials.add(metal);
    Object& crate = scene.objects.get(handle);
    crate.material = woodHandle;
    const std::vector<FaceHandle> faces = crate.meshData.getFaceHandles();
    crate.meshData.setFaceMaterial(faces[1], metalHandle);
    crate.meshData.setFaceMaterial(faces[4], metalHandle);

    ProjectFile::View view;
    Scene loaded;
    ProjectFile::View loadedView;
    std::string error;
    CHECK(ProjectFile::read(ProjectFile::write(scene, view), loaded, loadedView, error));

    const Object& back = loaded.objects.get(loaded.objects.handleAt(0));
    const std::vector<FaceHandle> loadedFaces = back.meshData.getFaceHandles();
    CHECK(loaded.materials.get(loaded.materials.resolve(back.material)).name == "Wood");
    u32 metalFaces = 0;
    for (std::size_t f = 0; f < loadedFaces.size(); ++f) {
        const MaterialHandle own = back.meshData.getFaceMaterial(loadedFaces[f]);
        if (loaded.materials.isValid(own) && loaded.materials.get(own).name == "Metal") {
            ++metalFaces;
            CHECK(f == 1 || f == 4);
        }
    }
    CHECK(metalFaces == 2);
}

TEST_CASE(asset_export_draws_one_part_per_material_and_reimports_face_materials) {
    ObjectCollection objects;
    MaterialCollection materials;
    Material paint;
    paint.name = "Paint";
    const MaterialHandle paintHandle = materials.add(paint);
    Material trim;
    trim.name = "Trim";
    trim.metallic = 1.0f;
    const MaterialHandle trimHandle = materials.add(trim);

    const ObjectHandle handle = objects.add("Box", PresetMesh::Cube);
    Object& box = objects.get(handle);
    box.material = paintHandle;
    const std::vector<FaceHandle> faces = box.meshData.getFaceHandles();
    box.meshData.setFaceMaterial(faces[0], trimHandle);
    box.meshData.setFaceMaterial(faces[3], trimHandle);

    const std::vector<u8> bytes = AssetFile::write(objects, handle, materials);
    vlmobj::File file;
    CHECK(file.open(bytes.data(), bytes.size()));
    const vlmobj::Mesh& mesh = file.meshes()[file.nodes()[0].mesh];
    CHECK(mesh.partCount == 2);
    std::set<std::string> partMaterials;
    u32 trimIndices = 0;
    for (u32 p = 0; p < mesh.partCount; ++p) {
        const vlmobj::Part& part = file.parts()[mesh.firstPart + p];
        const std::string name(file.string(file.materials()[part.material].name));
        partMaterials.insert(name);
        if (name == "Trim") trimIndices = part.indexCount;
    }
    CHECK(partMaterials == std::set<std::string>({ "Paint", "Trim" }));
    CHECK(trimIndices == 12);

    std::vector<AssetFile::ImportedObject> imported;
    std::vector<Material> importedMaterials;
    std::string error;
    CHECK(AssetFile::read(bytes, imported, importedMaterials, error));
    CHECK(importedMaterials[imported[0].material].name == "Paint");
    CHECK(imported[0].faceMaterials.size() == 6);
    CHECK(importedMaterials[imported[0].faceMaterials[0]].name == "Trim" && importedMaterials[imported[0].faceMaterials[3]].name == "Trim");
    CHECK(imported[0].faceMaterials[1] == vlmobj::NONE);
}
