#include "test.hpp"
#include "mesh_helpers.hpp"

namespace {
    MeshData preset(PresetMesh type) {
        MeshData mesh;
        mesh.setMesh(type);
        return mesh;
    }

    Vec3 faceCenter(const MeshData& mesh, FaceHandle face) {
        Vec3 sum(0.0f);
        const std::vector<VertexHandle> corners = mesh.getFaceVertices(face);
        for (VertexHandle corner : corners) sum += mesh.getVertexPosition(corner);
        return sum / static_cast<f32>(corners.size());
    }

    // Every face normal points away from the origin
    bool normalsPointOutward(const MeshData& mesh) {
        for (FaceHandle face : mesh.getFaceHandles()) {
            if (Vec3::dot(mesh.getFaceNormal(face), faceCenter(mesh, face)) <= 0.0f) return false;
        }
        return true;
    }

    bool normalsFaceUp(const MeshData& mesh) {
        for (FaceHandle face : mesh.getFaceHandles()) {
            if (mesh.getFaceNormal(face).y < 0.999f) return false;
        }
        return true;
    }

    u32 borderEdgeCount(const MeshData& mesh) {
        u32 count = 0;
        for (EdgeHandle edge : mesh.getEdgeHandles()) {
            if (mesh.isBorder(edge)) ++count;
        }
        return count;
    }
}

TEST_CASE(preset_plane) {
    MeshData mesh = preset(PresetMesh::Plane);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 4, 8, 1));
    CHECK(borderEdgeCount(mesh) == 4);
    CHECK(normalsFaceUp(mesh));
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(preset_grid) {
    MeshData mesh = preset(PresetMesh::Grid);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 11 * 11, 2 * (2 * 10 * 11), 100));
    CHECK(borderEdgeCount(mesh) == 40);
    CHECK(normalsFaceUp(mesh));
}

TEST_CASE(preset_circle) {
    MeshData mesh = preset(PresetMesh::Circle);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 32, 64, 1));
    CHECK(borderEdgeCount(mesh) == 32);
    CHECK(normalsFaceUp(mesh));
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(preset_cylinder) {
    MeshData mesh = preset(PresetMesh::Cylinder);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 32, 2 * 48, 18));
    CHECK(borderEdgeCount(mesh) == 0);
    CHECK(normalsPointOutward(mesh));
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(preset_cone) {
    MeshData mesh = preset(PresetMesh::Cone);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 17, 2 * 32, 17));
    CHECK(borderEdgeCount(mesh) == 0);
    CHECK(normalsPointOutward(mesh));
    CHECK(everyFaceTriangulates(mesh));
}

TEST_CASE(preset_uv_sphere) {
    MeshData mesh = preset(PresetMesh::UVSphere);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 2 + 16 * 7, 2 * (16 * 7 + 16 * 8), 16 * 8));
    CHECK(borderEdgeCount(mesh) == 0);
    CHECK(normalsPointOutward(mesh));
}

TEST_CASE(preset_ico_sphere) {
    MeshData mesh = preset(PresetMesh::IcoSphere);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 42, 2 * 120, 80));
    CHECK(borderEdgeCount(mesh) == 0);
    CHECK(normalsPointOutward(mesh));
}

TEST_CASE(preset_torus) {
    MeshData mesh = preset(PresetMesh::Torus);
    CHECK(mesh.validate());
    CHECK(counts(mesh, 24 * 12, 2 * (2 * 24 * 12), 24 * 12));
    CHECK(borderEdgeCount(mesh) == 0);

    // Normals point away from the center of the tube, not the origin
    for (FaceHandle face : mesh.getFaceHandles()) {
        const Vec3 center = faceCenter(mesh, face);
        const Vec3 tubeCenter = Vec3(center.x, 0.0f, center.z).normalized() * 0.4f;
        CHECK(Vec3::dot(mesh.getFaceNormal(face), center - tubeCenter) > 0.0f);
    }
}

TEST_CASE(preset_extrude_and_inset_stay_valid) {
    for (PresetMesh type : { PresetMesh::Plane, PresetMesh::Circle, PresetMesh::Cylinder, PresetMesh::Cone,
                             PresetMesh::UVSphere, PresetMesh::IcoSphere, PresetMesh::Torus }) {
        MeshData mesh = preset(type);
        const FaceHandle face = mesh.getFaceHandles()[0];

        extrude(mesh, face);
        CHECK(mesh.validate());
        CHECK(mesh.isValidHandle(mesh.insertFaceRing(face)));
        CHECK(mesh.validate());
    }
}
