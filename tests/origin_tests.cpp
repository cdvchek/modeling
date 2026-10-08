#include "test.hpp"
#include "mesh_helpers.hpp"
#include "scene/objects/origin.hpp"
#include "scene/selection/selection.hpp"

#include <cmath>

namespace {
    ObjectHandle addTurnedCube(ObjectCollection& objects) {
        const ObjectHandle handle = objects.add("Cube", PresetMesh::Cube);
        Transform& transform = objects.get(handle).transform;
        transform.position = Vec3(2.0f, 1.0f, -1.0f);
        transform.rotation = Vec3(0.3f, -0.6f, 0.9f);
        transform.scale = Vec3(1.0f, 2.0f, 0.5f);
        return handle;
    }

    std::vector<Vec3> worldPositions(const ObjectCollection& objects, ObjectHandle handle) {
        const ObjectSpace space(objects.worldTransform(handle));
        std::vector<Vec3> result;
        for (const Vec3& p : vertexPositions(objects.get(handle).meshData)) result.push_back(space.pointToWorld(p));
        return result;
    }

    bool same(const Vec3& a, const Vec3& b, f32 tolerance = 1e-4f) {
        return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance && std::fabs(a.z - b.z) <= tolerance;
    }

    bool sameWorld(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (!same(a[i], b[i])) return false;
        return true;
    }
}

TEST_CASE(origin_moves_leave_the_mesh_in_place) {
    ObjectCollection objects;
    const ObjectHandle cube = addTurnedCube(objects);
    const std::vector<Vec3> before = worldPositions(objects, cube);

    for (Transform (*target)(const ObjectCollection&, ObjectHandle) : { &originAtCenter, &originAtBottom, &originAtWorld, &originAlignedToWorld }) {
        setOrigin(objects, cube, target(objects, cube));
        CHECK(sameWorld(worldPositions(objects, cube), before));
        CHECK(objects.get(cube).meshDirty);
    }

    // Moved and turned freely, as grab and rotate do
    Transform free = objects.worldTransform(cube);
    free.position = Vec3(-3.0f, 4.0f, 0.5f);
    free.rotation = Vec3(1.2f, 0.1f, -0.4f);
    setOrigin(objects, cube, free);
    CHECK(sameWorld(worldPositions(objects, cube), before));
    CHECK(same(objects.worldTransform(cube).position, free.position));
}

TEST_CASE(origin_targets) {
    ObjectCollection objects;
    const ObjectHandle cube = objects.add("Cube", PresetMesh::Cube);
    objects.get(cube).transform.position = Vec3(5.0f, 0.0f, 0.0f);

    // A cube is centered on its origin; the bottom is half a unit down
    CHECK(same(originAtCenter(objects, cube).position, Vec3(5.0f, 0.0f, 0.0f)));
    CHECK(same(originAtBottom(objects, cube).position, Vec3(5.0f, -0.5f, 0.0f)));
    CHECK(same(originAtWorld(objects, cube).position, Vec3(0.0f)));

    // Standing on its new origin: the lowest vertices sit at local y = 0
    setOrigin(objects, cube, originAtBottom(objects, cube));
    f32 lowest = 1e9f;
    for (const Vec3& p : vertexPositions(objects.get(cube).meshData)) lowest = std::min(lowest, p.y);
    CHECK(std::fabs(lowest) < 1e-5f);

    // The average of picked vertices
    const MeshData& mesh = objects.get(cube).meshData;
    const std::vector<VertexHandle> vertices = mesh.getVertexHandles();
    const Vec3 a = mesh.getVertexPosition(vertices[0]);
    const Vec3 b = mesh.getVertexPosition(vertices[1]);
    const Transform middle = originAtVertices(objects, cube, { vertices[0], vertices[1] });
    CHECK(same(middle.position, objects.worldTransform(cube).position + (a + b) * 0.5f));

    // Resetting the rotation keeps the position
    objects.get(cube).transform.rotation = Vec3(0.5f, 0.0f, 0.0f);
    const Transform aligned = originAlignedToWorld(objects, cube);
    CHECK(same(aligned.rotation, Vec3(0.0f)) && same(aligned.position, objects.worldTransform(cube).position));
}

TEST_CASE(origin_drag_from_the_start_does_not_drift) {
    ObjectCollection dragged;
    const ObjectHandle a = addTurnedCube(dragged);
    const OriginStart start = captureOrigin(dragged, a);

    // Many small steps, each from the start, end exactly where one jump does
    Transform to = start.world;
    for (int i = 0; i < 200; ++i) {
        to.position += Vec3(0.01f, -0.02f, 0.005f);
        to.rotation = rotateEuler(start.world.rotation, Vec3(0.0f, 1.0f, 0.0f), 0.01f * static_cast<f32>(i));
        setOrigin(dragged, a, start, to);
    }

    ObjectCollection jumped;
    const ObjectHandle b = addTurnedCube(jumped);
    setOrigin(jumped, b, to);
    CHECK(sameWorld(vertexPositions(dragged.get(a).meshData), vertexPositions(jumped.get(b).meshData)));
}

TEST_CASE(origin_of_a_parent_leaves_its_children_in_place) {
    ObjectCollection objects;
    const ObjectHandle parent = addTurnedCube(objects);
    const ObjectHandle child = objects.add("Wheel", PresetMesh::Cylinder);
    objects.get(child).transform.position = Vec3(1.0f, 0.0f, 0.5f);
    objects.setParent(child, parent);

    const Transform childBefore = objects.worldTransform(child);
    const std::vector<Vec3> childMesh = worldPositions(objects, child);

    setOrigin(objects, parent, originAtBottom(objects, parent));
    Transform turned = objects.worldTransform(parent);
    turned.rotation = Vec3(0.0f, 1.0f, 0.0f);
    setOrigin(objects, parent, turned);

    CHECK(same(objects.worldTransform(child).position, childBefore.position));
    CHECK(sameWorld(worldPositions(objects, child), childMesh));
    CHECK(objects.parentOf(child) == parent);
}

TEST_CASE(origin_selection_never_mixes) {
    ObjectCollection objects;
    const ObjectHandle cube = objects.add("Cube", PresetMesh::Cube);
    const ObjectHandle plane = objects.add("Plane", PresetMesh::Plane);
    const VertexHandle vertex = objects.get(cube).meshData.getVertexHandles()[0];

    Selection selection;
    selection.setActiveObject(plane);
    selection.addLight({ 0, 0 });
    selection.selectObject(plane);

    // Selecting an origin makes its object active and clears everything else
    selection.selectOrigin(cube);
    CHECK(selection.getOrigin() == cube && selection.getActiveObject() == cube);
    CHECK(!selection.hasLights() && !selection.hasObjects() && !selection.hasVertices());

    // Anything else clears it
    selection.addVertex(cube, vertex);
    CHECK(!selection.hasOrigin());

    selection.selectOrigin(cube);
    selection.addLight({ 0, 0 });
    CHECK(!selection.hasOrigin());

    selection.selectOrigin(cube);
    selection.selectObject(plane);
    CHECK(!selection.hasOrigin());

    selection.selectOrigin(cube);
    selection.setActiveObject(plane);
    CHECK(!selection.hasOrigin());

    selection.selectOrigin(cube);
    selection.clear();
    CHECK(!selection.hasOrigin() && selection.getActiveObject() == cube);
}
