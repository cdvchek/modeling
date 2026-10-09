#include "test.hpp"
#include "mesh_helpers.hpp"
#include "asset/asset_file.hpp"
#include "project/project_file.hpp"
#include "core/math/math_utils.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace {
    MeshData preset(PresetMesh type, ShadingMode mode) {
        MeshData mesh;
        mesh.setMesh(type);
        mesh.setShading(mode);
        return mesh;
    }

    bool sameVec3(const Vec3& a, const Vec3& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    bool nearVec3(const Vec3& a, const Vec3& b, f32 tolerance = 1e-4f) {
        return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance && std::fabs(a.z - b.z) <= tolerance;
    }

    // Each face's drawn corners: position and normal per triangle corner
    std::vector<std::vector<f32>> faceCorners(const MeshData& mesh) {
        std::vector<std::vector<f32>> result;
        for (FaceHandle face : mesh.getFaceHandles()) {
            std::vector<f32> corners;
            mesh.appendFaceCorners(face, corners);
            result.push_back(std::move(corners));
        }
        return result;
    }

    // Normals drawn at each position, grouped by position
    std::map<std::tuple<f32, f32, f32>, std::vector<Vec3>> normalsByPosition(const MeshData& mesh) {
        std::map<std::tuple<f32, f32, f32>, std::vector<Vec3>> result;
        for (const std::vector<f32>& corners : faceCorners(mesh)) {
            for (std::size_t at = 0; at + FaceData::FLOATS_PER_VERTEX <= corners.size(); at += FaceData::FLOATS_PER_VERTEX) {
                result[{ corners[at], corners[at + 1], corners[at + 2] }].push_back(Vec3(corners[at + 3], corners[at + 4], corners[at + 5]));
            }
        }
        return result;
    }

    u32 markedEdges(const MeshData& mesh, EdgeMark mark) {
        u32 halves = 0;
        for (EdgeMark m : mesh.getEdgeMarks()) if (m == mark) ++halves;
        return halves / 2;
    }

    // The half-edge from a to b
    EdgeHandle edgeBetween(const MeshData& mesh, VertexHandle a, VertexHandle b) {
        return findEdgeBetween(mesh, a, b);
    }
}

TEST_CASE(flat_shading_draws_face_normals) {
    const MeshData mesh = cube();
    CHECK(mesh.getShading() == ShadingMode::Flat);
    CHECK(mesh.getHardEdges().empty());

    bool faceNormals = true;
    const std::vector<FaceHandle> faces = mesh.getFaceHandles();
    const std::vector<std::vector<f32>> corners = faceCorners(mesh);
    for (std::size_t f = 0; f < faces.size(); ++f) {
        const Vec3 normal = mesh.getFaceNormal(faces[f]);
        for (std::size_t at = 0; at < corners[f].size(); at += FaceData::FLOATS_PER_VERTEX) {
            faceNormals = faceNormals && sameVec3(Vec3(corners[f][at + 3], corners[f][at + 4], corners[f][at + 5]), normal);
        }
    }
    CHECK(faceNormals);
}

TEST_CASE(smooth_shading_shares_one_normal_per_vertex) {
    // A smooth cube: each corner's three faces average to the diagonal, bit for bit the same in all three
    const MeshData mesh = preset(PresetMesh::Cube, ShadingMode::Smooth);
    bool diagonal = true, identical = true;
    for (const auto& [position, normals] : normalsByPosition(mesh)) {
        const Vec3 expected = Vec3(std::get<0>(position), std::get<1>(position), std::get<2>(position)).normalized();
        for (const Vec3& normal : normals) {
            diagonal = diagonal && nearVec3(normal, expected);
            identical = identical && sameVec3(normal, normals[0]);
        }
    }
    CHECK(diagonal);
    CHECK(identical);
    CHECK(mesh.getHardEdges().empty());

    // A torus has no borders: every vertex's fan closes on itself and still gives one exact normal
    const MeshData torus = preset(PresetMesh::Torus, ShadingMode::Smooth);
    bool torusIdentical = true;
    for (const auto& [position, normals] : normalsByPosition(torus)) {
        for (const Vec3& normal : normals) torusIdentical = torusIdentical && sameVec3(normal, normals[0]);
    }
    CHECK(torusIdentical);
}

TEST_CASE(auto_shading_keeps_edges_sharper_than_its_angle) {
    // The cube's faces meet at 90 degrees: hard below that, smooth above
    MeshData mesh = preset(PresetMesh::Cube, ShadingMode::Auto);
    CHECK(mesh.getHardEdges().size() == 12);
    // Each corner shows its three faces' normals
    bool threeEach = true;
    for (const auto& [position, normals] : normalsByPosition(mesh)) {
        std::vector<Vec3> distinct;
        for (const Vec3& normal : normals) {
            if (std::none_of(distinct.begin(), distinct.end(), [&](const Vec3& seen) { return sameVec3(seen, normal); })) distinct.push_back(normal);
        }
        threeEach = threeEach && distinct.size() == 3;
    }
    CHECK(threeEach);

    mesh.setSmoothAngle(100.0f * Math::PI / 180.0f);
    CHECK(mesh.getHardEdges().empty());

    // A UV sphere's neighbouring faces meet at far less than 30 degrees, so it's all smooth
    const MeshData sphere = preset(PresetMesh::UVSphere, ShadingMode::Auto);
    CHECK(sphere.getHardEdges().empty());

    // The cylinder's caps meet its sides at 90 degrees: those 32 edges stay hard
    const MeshData cylinder = preset(PresetMesh::Cylinder, ShadingMode::Auto);
    CHECK(cylinder.getHardEdges().size() == 32);
}

TEST_CASE(edge_marks_override_the_mode) {
    MeshData mesh = preset(PresetMesh::Cube, ShadingMode::Smooth);
    const std::vector<VertexHandle> top = mesh.getFaceVertices(mesh.getFaceHandles()[0]);
    const EdgeHandle edge = edgeBetween(mesh, top[0], top[1]);
    const EdgeHandle other = edgeBetween(mesh, top[3], top[0]);
    CHECK(mesh.isValidHandle(edge) && mesh.isValidHandle(other));

    // Both halves carry the mark; one hard edge alone doesn't split a corner (its fan still joins round the third face)
    mesh.setEdgeMark(edge, EdgeMark::Hard);
    CHECK(mesh.getEdgeMark(edge) == EdgeMark::Hard);
    CHECK(mesh.getEdgeMark(mesh.getEdge(edge)->pair) == EdgeMark::Hard);
    CHECK(mesh.getHardEdges().size() == 1);
    CHECK(markedEdges(mesh, EdgeMark::Hard) == 1);

    // Two hard edges at the corner cut the top face off from the sides
    mesh.setEdgeMark(other, EdgeMark::Hard);
    CHECK(mesh.getHardEdges().size() == 2);

    const Vec3 end = mesh.getVertexPosition(top[0]);
    const auto normals = normalsByPosition(mesh)[{ end.x, end.y, end.z }];
    bool split = false;
    for (const Vec3& normal : normals) split = split || !sameVec3(normal, normals[0]);
    CHECK(split);

    // In auto mode a smooth mark softens an edge sharper than the angle
    mesh.setShading(ShadingMode::Auto);
    mesh.setEdgeMark(edge, EdgeMark::Smooth);
    CHECK(mesh.getHardEdges().size() == 11);

    // Flat ignores marks
    mesh.setShading(ShadingMode::Flat);
    CHECK(mesh.getHardEdges().empty());
}

TEST_CASE(edge_marks_survive_split_and_bevel) {
    MeshData mesh = preset(PresetMesh::Cube, ShadingMode::Smooth);
    for (EdgeHandle edge : mesh.getEdgeHandles()) mesh.setEdgeMark(edge, EdgeMark::Hard);
    CHECK(markedEdges(mesh, EdgeMark::Hard) == 12);

    // Both pieces of a split edge keep its mark
    mesh.splitEdge(mesh.getEdgeHandles()[0]);
    CHECK(mesh.validate());
    CHECK(markedEdges(mesh, EdgeMark::Hard) == 13);

    // A beveled corner rebuilds its three faces; edges between the same two vertices keep their marks
    MeshData beveled = preset(PresetMesh::Cube, ShadingMode::Smooth);
    for (EdgeHandle edge : beveled.getEdgeHandles()) beveled.setEdgeMark(edge, EdgeMark::Hard);
    SlideSession session;
    CHECK(beveled.bevelVertex(beveled.getVertexHandles()[0], session));
    beveled.setSlideWidth(session, 0.25f);
    CHECK(beveled.validate());
    CHECK(markedEdges(beveled, EdgeMark::Hard) == 9);
    bool pairsAgree = true;
    for (EdgeHandle edge : beveled.getEdgeHandles()) pairsAgree = pairsAgree && beveled.getEdgeMark(edge) == beveled.getEdgeMark(beveled.getEdge(edge)->pair);
    CHECK(pairsAgree);
}

TEST_CASE(smooth_patches_cover_every_changed_face) {
    MeshData mesh = preset(PresetMesh::UVSphere, ShadingMode::Smooth);
    const std::vector<std::vector<f32>> before = faceCorners(mesh);

    const VertexHandle moved = mesh.getVertexHandles()[20];
    mesh.translateVertex(moved, Vec3(0.05f, 0.1f, -0.02f));
    const std::vector<std::vector<f32>> after = faceCorners(mesh);
    const std::vector<FaceHandle> patched = mesh.getFacesToPatch({ moved });

    // Every face whose drawn corners changed is in the patch
    const std::vector<FaceHandle> faces = mesh.getFaceHandles();
    bool covered = true;
    u32 changed = 0;
    for (std::size_t f = 0; f < faces.size(); ++f) {
        if (before[f] == after[f]) continue;
        ++changed;
        covered = covered && std::find(patched.begin(), patched.end(), faces[f]) != patched.end();
    }
    CHECK(changed > mesh.getVertexFaces(moved).size());
    CHECK(covered);

    // Flat only needs the faces around the vertex
    mesh.setShading(ShadingMode::Flat);
    CHECK(mesh.getFacesToPatch({ moved }).size() == mesh.getVertexFaces(moved).size());
}

TEST_CASE(smooth_export_shares_vertices_and_round_trips) {
    Object object;
    object.name = "Donut";
    object.meshData.setMesh(PresetMesh::Torus);
    const u32 flatVertices = static_cast<u32>(AssetFile::bake(object.meshData).vertices.size() / 8);

    object.meshData.setShading(ShadingMode::Auto);
    object.meshData.setSmoothAngle(0.7f);
    object.meshData.setEdgeMark(object.meshData.getEdgeHandles()[5], EdgeMark::Hard);
    const u32 smoothVertices = static_cast<u32>(AssetFile::bake(object.meshData).vertices.size() / 8);
    CHECK(smoothVertices < flatVertices);

    Object loaded;
    std::string error;
    CHECK(AssetFile::read(AssetFile::write(object), loaded, error));
    CHECK(loaded.meshData.getShading() == ShadingMode::Auto);
    CHECK(loaded.meshData.getSmoothAngle() == 0.7f);
    CHECK(markedEdges(loaded.meshData, EdgeMark::Hard) == 1);
    CHECK(loaded.meshData.getHardEdges().size() == object.meshData.getHardEdges().size());
}

TEST_CASE(project_keeps_shading_and_marks) {
    Scene scene;
    const ObjectHandle handle = scene.objects.add("Ball", PresetMesh::UVSphere);
    MeshData& mesh = scene.objects.get(handle).meshData;
    mesh.setShading(ShadingMode::Auto);
    mesh.setSmoothAngle(0.4f);
    mesh.setEdgeMark(mesh.getEdgeHandles()[3], EdgeMark::Hard);
    mesh.setEdgeMark(mesh.getEdgeHandles()[40], EdgeMark::Smooth);

    Scene loaded;
    ProjectFile::View view;
    std::string error;
    CHECK(ProjectFile::read(ProjectFile::write(scene, ProjectFile::View()), loaded, view, error));
    const MeshData& back = loaded.objects.get(loaded.objects.handleAt(0)).meshData;
    CHECK(back.getShading() == ShadingMode::Auto);
    CHECK(back.getSmoothAngle() == 0.4f);
    CHECK(markedEdges(back, EdgeMark::Hard) == 1);
    CHECK(markedEdges(back, EdgeMark::Smooth) == 1);

    // A mesh without marks writes none and reads back flat
    Scene plain;
    plain.objects.add("Box", PresetMesh::Cube);
    Scene plainLoaded;
    CHECK(ProjectFile::read(ProjectFile::write(plain, ProjectFile::View()), plainLoaded, view, error));
    CHECK(plainLoaded.objects.get(plainLoaded.objects.handleAt(0)).meshData.getShading() == ShadingMode::Flat);
}
