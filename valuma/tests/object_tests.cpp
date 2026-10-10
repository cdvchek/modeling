#include "test.hpp"
#include "scene/objects/object_collection.hpp"
#include "scene/selection/selection.hpp"

TEST_CASE(objects_add_get_remove) {
    ObjectCollection objects;
    const ObjectHandle cube = objects.add("Cube", PresetMesh::Cube);
    const ObjectHandle plane = objects.add("Plane", PresetMesh::Plane);

    CHECK(objects.count() == 2);
    CHECK(objects.get(cube).meshData.getFaceHandles().size() == 6);
    CHECK(objects.get(plane).name == "Plane");
    CHECK(objects.handleAt(plane.index) == plane);

    objects.remove(cube);
    CHECK(!objects.isValid(cube));
    CHECK(objects.tryGet(cube) == nullptr);
    CHECK(objects.count() == 1);

    // The freed slot is reused, but the old handle stays invalid
    const ObjectHandle torus = objects.add("Torus", PresetMesh::Torus);
    CHECK(torus.index == cube.index);
    CHECK(torus != cube);
}

TEST_CASE(objects_unique_names) {
    ObjectCollection objects;
    CHECK(objects.uniqueName("Cube") == "Cube");

    objects.add("Cube", PresetMesh::Cube);
    CHECK(objects.uniqueName("Cube") == "Cube 2");

    objects.add("Cube 2", PresetMesh::Cube);
    CHECK(objects.uniqueName("Cube") == "Cube 3");
    CHECK(objects.uniqueName("Torus") == "Torus");
}

TEST_CASE(objects_copy_is_independent) {
    ObjectCollection objects;
    const ObjectHandle cube = objects.add("Cube", PresetMesh::Cube);

    ObjectCollection snapshot = objects;
    objects.get(cube).transform.position.x = 5.0f;
    objects.remove(cube);

    CHECK(snapshot.isValid(cube));
    CHECK(snapshot.get(cube).transform.position.x == 0.0f);
}

TEST_CASE(selection_changing_active_object_clears_elements) {
    ObjectCollection objects;
    const ObjectHandle cube = objects.add("Cube", PresetMesh::Cube);
    const ObjectHandle plane = objects.add("Plane", PresetMesh::Plane);

    Selection selection;
    selection.setActiveObject(cube);
    selection.addVertex(cube, objects.get(cube).meshData.getVertexHandles()[0]);
    selection.addLight({ 0, 0 });

    // Same object again keeps the selection
    selection.setActiveObject(cube);
    CHECK(selection.hasVertices());

    selection.setActiveObject(plane);
    CHECK(selection.getActiveObject() == plane);
    CHECK(!selection.hasVertices());
    CHECK(selection.hasLights());

    // clear() keeps the active object
    selection.clear();
    CHECK(selection.getActiveObject() == plane);
}
