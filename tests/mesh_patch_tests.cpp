#include "test.hpp"
#include "mesh_helpers.hpp"
#include "scene/objects/object_collection.hpp"

#include <cstring>

TEST_CASE(moving_vertices_keeps_the_stamp_and_records_them) {
    MeshData mesh = cube();
    const MeshStamp before = mesh.stamp();
    const VertexHandle vertex = mesh.getVertexHandles()[3];

    mesh.translateVertex(vertex, Vec3(0.1f, 0.0f, 0.0f));
    mesh.positionVertex(mesh.getVertexHandles()[5], Vec3(1.0f));
    CHECK(mesh.stamp() == before);
    CHECK(mesh.movedVertices().size() == 2 && mesh.movedVertices()[0] == vertex);
    CHECK(!mesh.allMoved());

    mesh.clearMoved();
    CHECK(mesh.movedVertices().empty());

    // A tool moving the same vertices frame after frame switches to "all" rather than growing without end
    for (int frame = 0; frame < 100; ++frame) {
        for (VertexHandle handle : mesh.getVertexHandles()) mesh.translateVertex(handle, Vec3(0.001f));
    }
    CHECK(mesh.allMoved());
    CHECK(mesh.movedVertices().empty());
}

TEST_CASE(layout_changes_give_a_new_stamp_and_copies_keep_theirs) {
    MeshData mesh = cube();
    const MeshStamp first = mesh.stamp();

    // A copy (an undo snapshot) holds the same layout
    const MeshData copy = mesh;
    CHECK(copy.stamp() == first);

    extrude(mesh, mesh.getFaceHandles()[0]);
    const MeshStamp extruded = mesh.stamp();
    CHECK(!(extruded == first));

    mesh.setFaceMaterial(mesh.getFaceHandles()[1], MaterialHandle { 1, 0 });
    CHECK(!(mesh.stamp() == extruded));

    // Restoring the copy goes back to its stamp, which differs from what was drawn last
    mesh = copy;
    CHECK(mesh.stamp() == first);

    // Two meshes built separately never share a stamp
    const MeshData other = cube();
    CHECK(!(other.stamp() == first));
}

TEST_CASE(face_corners_match_the_full_face_data) {
    MeshData mesh = cube();
    extrude(mesh, mesh.getFaceHandles()[2]);
    mesh.translateVertex(mesh.getVertexHandles()[0], Vec3(0.2f, -0.1f, 0.05f));

    const FaceData data = mesh.getFaceData();
    bool same = true;
    for (FaceHandle face : mesh.getFaceHandles()) {
        std::vector<f32> corners;
        mesh.appendFaceCorners(face, corners);
        const u32 first = data.indexMap[face.index * 2];
        const u32 count = data.indexMap[face.index * 2 + 1];
        same = same && corners.size() == count * FaceData::FLOATS_PER_VERTEX
            && std::memcmp(corners.data(), data.vertices.data() + first * FaceData::FLOATS_PER_VERTEX, corners.size() * sizeof(f32)) == 0;
    }
    CHECK(same);
}

TEST_CASE(vertex_faces_lists_every_face_around_a_vertex) {
    MeshData mesh = cube();
    for (VertexHandle vertex : mesh.getVertexHandles()) CHECK(mesh.getVertexFaces(vertex).size() == 3);

    // On an open mesh, a border vertex has fewer
    MeshData plane;
    plane.setMesh(PresetMesh::Plane);
    CHECK(plane.getVertexFaces(plane.getVertexHandles()[0]).size() == 1);
}

TEST_CASE(restoring_a_copy_marks_every_vertex_moved) {
    ObjectCollection objects;
    const ObjectHandle handle = objects.add("Cube", PresetMesh::Cube);
    const ObjectCollection snapshot = objects;

    // Moved and drawn, then the snapshot comes back with the same layout
    objects.get(handle).meshData.translateVertex(objects.get(handle).meshData.getVertexHandles()[0], Vec3(1.0f));
    objects.get(handle).meshData.clearMoved();
    objects = snapshot;
    objects.markAllDirty();

    CHECK(objects.get(handle).meshData.allMoved());
    CHECK(objects.get(handle).meshDirty);
}
