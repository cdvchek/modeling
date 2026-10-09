#include "test.hpp"
#include "scene/history.hpp"
#include "scene/scene.hpp"
#include "scene/picking/scene_queries.hpp"

#include <cmath>

namespace {
    Material granite() {
        Material material;
        material.name = "Granite";
        material.baseColor = Vec3(0.5f, 0.48f, 0.45f);
        material.roughness = 0.8f;
        return material;
    }
}

TEST_CASE(materials_start_with_a_default_that_cant_be_removed) {
    MaterialCollection materials;
    CHECK(materials.count() == 1);

    const MaterialHandle defaults = materials.defaultMaterial();
    CHECK(materials.isDefault(defaults));
    CHECK(materials.get(defaults).name == "Default");
    CHECK(materials.handles().front() == defaults);
    CHECK(!materials.remove(defaults));
    CHECK(materials.count() == 1);

    // Default can still be edited
    materials.get(defaults).roughness = 0.2f;
    CHECK(materials.get(defaults).roughness == 0.2f);
}

TEST_CASE(materials_resolve_missing_handles_to_default) {
    MaterialCollection materials;
    const MaterialHandle stone = materials.add(granite());

    CHECK(materials.resolve(stone) == stone);
    CHECK(materials.resolve(INVALID_MATERIAL) == materials.defaultMaterial());

    // A removed material's handle stays stale even when its slot is reused
    CHECK(materials.remove(stone));
    const MaterialHandle reused = materials.add(granite());
    CHECK(reused.index == stone.index);
    CHECK(materials.resolve(stone) == materials.defaultMaterial());
    CHECK(materials.resolve(reused) == reused);
}

TEST_CASE(materials_names_and_identical_lookup) {
    MaterialCollection materials;
    materials.add(granite());

    CHECK(materials.uniqueName("Granite") == "Granite 2");
    CHECK(materials.uniqueName("Default") == "Default 2");
    CHECK(materials.uniqueName("Marble") == "Marble");

    // Identical means the same name and every value
    Material same = granite();
    CHECK(!materials.findIdentical(same).isNull());
    Material tweaked = granite();
    tweaked.metallic = 0.1f;
    CHECK(materials.findIdentical(tweaked).isNull());
    Material renamed = granite();
    renamed.name = "Stone";
    CHECK(materials.findIdentical(renamed).isNull());
    CHECK(renamed.sameLook(granite()));

    Material doubleSided = granite();
    doubleSided.doubleSided = true;
    CHECK(!doubleSided.sameLook(granite()));
    Material blended = granite();
    blended.alphaMode = AlphaMode::Blend;
    CHECK(!blended.sameLook(granite()));
}

TEST_CASE(removing_a_material_puts_its_objects_back_on_default_and_undo_restores_it) {
    Scene scene;
    History history;
    const MaterialHandle stone = scene.materials.add(granite());
    const ObjectHandle rock = scene.objects.add("Rock", PresetMesh::Cube);
    const ObjectHandle other = scene.objects.add("Other", PresetMesh::Cube);
    scene.objects.get(rock).material = stone;
    scene.objects.get(other).material = stone;

    // New objects use Default
    const ObjectHandle plain = scene.objects.add("Plain", PresetMesh::Cube);
    CHECK(scene.materials.resolve(scene.objects.get(plain).material) == scene.materials.defaultMaterial());

    // Shared: editing it changes it for both
    scene.materials.get(stone).baseColor = Vec3(1.0f, 0.0f, 0.0f);
    CHECK(scene.materials.get(scene.materials.resolve(scene.objects.get(other).material)).baseColor.x == 1.0f);

    history.begin(scene);
    CHECK(scene.materials.remove(stone));
    history.commit();
    CHECK(scene.materials.resolve(scene.objects.get(rock).material) == scene.materials.defaultMaterial());

    CHECK(history.undo(scene));
    CHECK(scene.materials.resolve(scene.objects.get(rock).material) == stone);
    CHECK(scene.materials.get(stone).name == "Granite");
}

TEST_CASE(pick_face_skips_culled_back_faces) {
    Scene scene;
    const ObjectHandle cube = scene.objects.add("Cube", PresetMesh::Cube);

    // From inside the cube every face is seen from behind
    const Ray inside { Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f) };
    CHECK(pickFace(scene, inside).hit);
    const BackFacesCulled culled = [cube](ObjectHandle handle) { return handle == cube; };
    CHECK(!pickFace(scene, inside, INVALID_OBJECT, INVALID_OBJECT, culled).hit);

    // From outside, the near face is a front face and still hit
    const Ray outside { Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, -1.0f) };
    const FaceHit hit = pickFace(scene, outside, INVALID_OBJECT, INVALID_OBJECT, culled);
    CHECK(hit.hit);
    CHECK(std::abs(hit.distance - 4.5f) < 1e-4f);
}
