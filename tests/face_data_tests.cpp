#include "test.hpp"
#include "mesh_helpers.hpp"

namespace {
    Vec3 cornerPosition(const FaceData& data, u32 corner) {
        const f32* v = &data.vertices[corner * FaceData::FLOATS_PER_VERTEX];
        return Vec3(v[0], v[1], v[2]);
    }

    Vec3 cornerNormal(const FaceData& data, u32 corner) {
        const f32* v = &data.vertices[corner * FaceData::FLOATS_PER_VERTEX + 3];
        return Vec3(v[0], v[1], v[2]);
    }

    // Every triangle winds counter-clockwise around its corners' normal
    bool trianglesMatchNormals(const FaceData& data) {
        for (u32 i = 0; i + 2 < data.indices.size(); i += 3) {
            const Vec3 a = cornerPosition(data, data.indices[i]);
            const Vec3 b = cornerPosition(data, data.indices[i + 1]);
            const Vec3 c = cornerPosition(data, data.indices[i + 2]);

            const Vec3 winding = Vec3::cross(b - a, c - a);
            if (Vec3::dot(winding, cornerNormal(data, data.indices[i])) <= 0.0f) return false;
        }
        return true;
    }
}

TEST_CASE(face_data_cube_layout) {
    MeshData mesh = cube();
    FaceData data = mesh.getFaceData();

    CHECK(data.vertices.size() == 6 * 2 * 3 * FaceData::FLOATS_PER_VERTEX);
    CHECK(data.indices.size() == 6 * 2 * 3);

    for (FaceHandle face : mesh.getFaceHandles()) {
        CHECK(data.indexMap[face.index * 2 + 1] == 6);
    }
}

TEST_CASE(face_data_non_planar_quad_has_two_normals) {
    MeshData mesh = cube();
    const FaceHandle face = mesh.getFaceHandles()[0];

    const VertexHandle corner = mesh.getFaceVertices(face)[0];
    mesh.translateVertex(corner, mesh.getFaceNormal(face) * 0.5f);

    FaceData data = mesh.getFaceData();
    const u32 offset = data.indexMap[face.index * 2];

    const Vec3 first = cornerNormal(data, data.indices[offset]);
    const Vec3 second = cornerNormal(data, data.indices[offset + 3]);
    CHECK(Vec3::dot(first, second) < 0.999f);
    CHECK(trianglesMatchNormals(data));
}

TEST_CASE(face_data_normals_point_outward) {
    MeshData mesh = cube();
    FaceData data = mesh.getFaceData();

    const u32 cornerCount = static_cast<u32>(data.vertices.size() / FaceData::FLOATS_PER_VERTEX);
    for (u32 corner = 0; corner < cornerCount; ++corner) {
        const Vec3 normal = cornerNormal(data, corner);
        CHECK(std::abs(normal.length() - 1.0f) < 1e-4f);
        CHECK(Vec3::dot(normal, cornerPosition(data, corner)) > 0.0f);
    }
}

TEST_CASE(face_data_winding_matches_normals) {
    MeshData mesh = cube();
    CHECK(trianglesMatchNormals(mesh.getFaceData()));

    const FaceHandle face = mesh.getFaceHandles()[0];
    extrude(mesh, face);
    mesh.splitEdge(mesh.getFace(face)->edge);
    CHECK(trianglesMatchNormals(mesh.getFaceData()));
}
