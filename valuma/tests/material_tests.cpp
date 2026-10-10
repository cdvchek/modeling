#include "test.hpp"
#include "scene/history.hpp"
#include "scene/scene.hpp"
#include "scene/picking/scene_queries.hpp"
#include "scene/textures/paint_targets.hpp"
#include "core/math/vec4.hpp"

#include <algorithm>
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

TEST_CASE(pick_face_says_where_on_the_face) {
    Scene scene;
    const ObjectHandle cube = scene.objects.add("Cube", PresetMesh::Cube);
    const MeshData& mesh = scene.objects.get(cube).meshData;
    const auto worldPosition = [&](VertexHandle vertex) {
        const Vec3 local = mesh.getVertexPosition(vertex);
        const Vec4 world = scene.objects.worldMatrix(cube) * Vec4(local.x, local.y, local.z, 1.0f);
        return Vec3(world.x, world.y, world.z);
    };
    const auto near = [](Vec2 a, Vec2 b) { return std::abs(a.x - b.x) < 1e-4f && std::abs(a.y - b.y) < 1e-4f; };
    const auto rebuilt = [&](const FaceHit& hit) {
        const Vec3 point = worldPosition(hit.triangle.v0) * hit.weights[0] + worldPosition(hit.triangle.v1) * hit.weights[1] + worldPosition(hit.triangle.v2) * hit.weights[2];
        return (point - hit.point).length() < 1e-4f;
    };

    // Straight at the middle of the front face: the point on the ray, and the middle of the face's UVs
    const Ray middle { Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, -1.0f) };
    const FaceHit hit = pickFace(scene, middle);
    CHECK(hit.hit);
    CHECK((hit.point - Vec3(0.0f, 0.0f, 0.5f)).length() < 1e-4f);
    CHECK(std::abs(hit.weights[0] + hit.weights[1] + hit.weights[2] - 1.0f) < 1e-5f);
    CHECK(hit.weights[0] >= 0.0f && hit.weights[1] >= 0.0f && hit.weights[2] >= 0.0f);
    CHECK(rebuilt(hit));
    CHECK((hit.corners[0] - worldPosition(hit.triangle.v0)).length() < 1e-5f && (hit.corners[2] - worldPosition(hit.triangle.v2)).length() < 1e-5f);
    const std::vector<Vec2> uvs = mesh.getFaceUVs(hit.face);
    CHECK(uvs.size() == 4);
    CHECK(near(hit.uv, (uvs[0] + uvs[1] + uvs[2] + uvs[3]) * 0.25f));

    // Almost at one corner: that corner's UV, this face's own (the cube's corners sit on seams)
    const std::vector<VertexHandle> vertices = mesh.getFaceVertices(hit.face);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const Vec3 corner = worldPosition(vertices[i]);
        const Ray at { Vec3(corner.x * 0.9998f, corner.y * 0.9998f, 5.0f), Vec3(0.0f, 0.0f, -1.0f) };
        const FaceHit cornerHit = pickFace(scene, at);
        CHECK(cornerHit.hit && cornerHit.face == hit.face);
        CHECK(std::abs(cornerHit.uv.x - uvs[i].x) < 1e-3f && std::abs(cornerHit.uv.y - uvs[i].y) < 1e-3f);
    }

    // A quarter of the way across: a quarter of the way across its UVs, whichever triangle it lands in
    const Vec3 a = worldPosition(vertices[0]), b = worldPosition(vertices[1]), d = worldPosition(vertices[3]);
    const Vec3 quarter = a + (b - a) * 0.25f + (d - a) * 0.6f;
    const FaceHit quarterHit = pickFace(scene, { Vec3(quarter.x, quarter.y, 5.0f), Vec3(0.0f, 0.0f, -1.0f) });
    CHECK(quarterHit.hit && quarterHit.face == hit.face && rebuilt(quarterHit));
    CHECK(near(quarterHit.uv, uvs[0] + (uvs[1] - uvs[0]) * 0.25f + (uvs[3] - uvs[0]) * 0.6f));

    // Moving, turning, and scaling the object moves the point, not the UV
    Object& object = scene.objects.get(cube);
    object.transform.position = Vec3(3.0f, -1.0f, 2.0f);
    object.transform.rotation = Vec3(0.0f, 0.5f, 0.0f);
    object.transform.scale = Vec3(2.0f, 2.0f, 2.0f);
    const Vec3 moved = worldPosition(vertices[0]) + (worldPosition(vertices[1]) - worldPosition(vertices[0])) * 0.25f + (worldPosition(vertices[3]) - worldPosition(vertices[0])) * 0.6f;
    const Vec3 from(3.0f, -1.0f, 12.0f);
    const Vec3 toward = moved - from;
    const FaceHit movedHit = pickFace(scene, { from, toward * (1.0f / toward.length()) });
    CHECK(movedHit.hit && movedHit.face == hit.face && rebuilt(movedHit));
    CHECK((movedHit.point - moved).length() < 1e-3f);
    CHECK((movedHit.corners[1] - worldPosition(movedHit.triangle.v1)).length() < 1e-4f);
    CHECK(near(movedHit.uv, quarterHit.uv));

    // A face with more than four corners: the hit's triangle is one of its own, and its corner UVs are that face's
    Scene round;
    const ObjectHandle disc = round.objects.add("Circle", PresetMesh::Circle);
    const FaceHit discHit = pickFace(round, { Vec3(0.3f, 5.0f, 0.2f), Vec3(0.0f, -1.0f, 0.0f) });
    CHECK(discHit.hit);
    const std::vector<Vec2> discUVs = round.objects.get(disc).meshData.getFaceUVs(discHit.face);
    CHECK(discUVs.size() > 4);
    bool own = true;
    for (const Vec2& uv : discHit.cornerUVs) own = own && std::any_of(discUVs.begin(), discUVs.end(), [&](const Vec2& other) { return near(uv, other); });
    CHECK(own);
    CHECK(near(discHit.uv, discHit.cornerUVs[0] * discHit.weights[0] + discHit.cornerUVs[1] * discHit.weights[1] + discHit.cornerUVs[2] * discHit.weights[2]));

    // A miss leaves nothing
    CHECK(!pickFace(scene, { Vec3(50.0f, 50.0f, 50.0f), Vec3(0.0f, 1.0f, 0.0f) }).hit);
}

TEST_CASE(texture_points_scale_and_wrap) {
    const auto at = [](Vec2 uv, f32 x, f32 y) {
        const Vec2 point = texturePoint(uv, 200, 100);
        return std::abs(point.x - x) < 1e-3f && std::abs(point.y - y) < 1e-3f;
    };
    CHECK(at(Vec2(0.5f, 0.25f), 100.0f, 25.0f));
    CHECK(at(Vec2(0.0f, 0.0f), 0.0f, 0.0f));
    // The far edge is the far edge, not the near one again
    CHECK(at(Vec2(1.0f, 1.0f), 200.0f, 100.0f));
    // Past either end the texture repeats
    CHECK(at(Vec2(1.25f, 2.5f), 50.0f, 50.0f));
    CHECK(at(Vec2(-0.25f, -1.75f), 150.0f, 25.0f));
}
