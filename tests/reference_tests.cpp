#include "test.hpp"
#include "project/project_file.hpp"
#include "scene/history.hpp"
#include "scene/scene.hpp"
#include "scene/selection/scene_queries.hpp"
#include "core/math/vec4.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
    bool near(f32 a, f32 b, f32 tolerance = 1e-4f) {
        return std::abs(a - b) <= tolerance;
    }

    bool nearVec3(const Vec3& a, const Vec3& b, f32 tolerance = 1e-4f) {
        return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance) && near(a.z, b.z, tolerance);
    }

    Vec3 transformPoint(const Mat4& matrix, const Vec3& point) {
        const Vec4 result = matrix * Vec4(point.x, point.y, point.z, 1.0f);
        return Vec3(result.x, result.y, result.z);
    }

    Vec3 transformDirection(const Mat4& matrix, const Vec3& direction) {
        const Vec4 result = matrix * Vec4(direction.x, direction.y, direction.z, 0.0f);
        return Vec3(result.x, result.y, result.z);
    }

    // A 13 x 7 RGBA PNG from the image library's fixtures
    std::shared_ptr<const ReferencePicture> picture(u32 width = 13, u32 height = 7) {
        const auto path = std::filesystem::path(SOURCE_DIR) / "shared/image/tests/fixtures/rgba8.png";
        std::ifstream file(path, std::ios::binary);

        auto result = std::make_shared<ReferencePicture>();
        result->fileName = "rgba8.png";
        result->png.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        result->width = width;
        result->height = height;
        return result;
    }

    // An image 2 tall and 4 wide at the origin, facing +Z
    ReferenceImage wideImage() {
        ReferenceImage image;
        image.name = "Front";
        image.picture = picture(400, 200);
        return image;
    }

    Ray rayAlongMinusZ(f32 x, f32 y, f32 fromZ = 10.0f) {
        return { Vec3(x, y, fromZ), Vec3(0.0f, 0.0f, -1.0f) };
    }
}

TEST_CASE(reference_image_keeps_the_picture_proportions) {
    const ReferenceImage image = wideImage();
    CHECK(near(image.aspect(), 2.0f));

    // Size is the height; the width follows
    const Mat4 matrix = image.matrix();
    CHECK(nearVec3(transformPoint(matrix, Vec3(0.5f, 0.5f, 0.0f)), Vec3(2.0f, 1.0f, 0.0f)));
    CHECK(nearVec3(transformPoint(matrix, Vec3(-0.5f, -0.5f, 0.0f)), Vec3(-2.0f, -1.0f, 0.0f)));

    // No picture: square
    ReferenceImage empty;
    CHECK(near(empty.aspect(), 1.0f));
}

TEST_CASE(reference_collection_names_are_unique) {
    ReferenceCollection references;
    ReferenceImage image = wideImage();
    image.name = references.uniqueName("Front");
    references.add(image);
    image.name = references.uniqueName("Front");
    const ReferenceHandle second = references.add(image);

    CHECK(references.count() == 2);
    CHECK(references.get(second).name == "Front 2");
    CHECK(references.handleAt(1) == second);
    CHECK(!references.isValid(references.handleAt(5)));

    references.remove(second);
    CHECK(references.count() == 1);
    CHECK(references.uniqueName("Front") == "Front 2");
}

TEST_CASE(euler_from_axes_turns_an_image_to_face_a_view) {
    // Looking down -X from +X: the image's +Z (its front) must point back at the camera, its top up
    const Vec3 right(0.0f, 0.0f, -1.0f), up(0.0f, 1.0f, 0.0f), back(1.0f, 0.0f, 0.0f);
    Transform transform;
    transform.rotation = eulerFromAxes(right, up, back);
    const Mat4 matrix = transform.getMatrix();

    CHECK(nearVec3(transformDirection(matrix, Vec3(1.0f, 0.0f, 0.0f)), right));
    CHECK(nearVec3(transformDirection(matrix, Vec3(0.0f, 1.0f, 0.0f)), up));
    CHECK(nearVec3(transformDirection(matrix, Vec3(0.0f, 0.0f, 1.0f)), back));

    // A camera looking straight down: top of the image toward -Z
    transform.rotation = eulerFromAxes(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f));
    CHECK(nearVec3(transformDirection(transform.getMatrix(), Vec3(0.0f, 0.0f, 1.0f)), Vec3(0.0f, 1.0f, 0.0f)));
    CHECK(nearVec3(transformDirection(transform.getMatrix(), Vec3(0.0f, 1.0f, 0.0f)), Vec3(0.0f, 0.0f, -1.0f)));
}

TEST_CASE(reference_selection_never_mixes_with_other_kinds) {
    Scene scene;
    const ObjectHandle cube = scene.objects.add("Cube", PresetMesh::Cube);
    const LightHandle light = scene.lights.add(Light {});
    const ReferenceHandle image = scene.references.add(wideImage());
    Selection& selection = scene.selection;
    selection.setActiveObject(cube);

    selection.addLight(light);
    selection.selectObject(cube);
    selection.addReference(image);
    CHECK(selection.hasReference(image));
    CHECK(!selection.hasLights());
    CHECK(!selection.hasObjects());
    CHECK(selection.getActiveObject() == cube);

    selection.addLight(light);
    CHECK(!selection.hasReferences());

    selection.addReference(image);
    selection.selectOrigin(cube);
    CHECK(!selection.hasReferences());

    selection.addReference(image);
    selection.addVertex(cube, scene.objects.get(cube).meshData.getVertexHandles()[0]);
    CHECK(!selection.hasReferences());

    selection.addReference(image);
    selection.clear();
    CHECK(!selection.hasReferences());
}

TEST_CASE(pick_reference_hits_the_picture_only) {
    Scene scene;
    const ReferenceHandle handle = scene.references.add(wideImage());

    ReferenceHit hit = pickReference(scene, rayAlongMinusZ(1.9f, 0.9f));
    CHECK(hit.hit);
    CHECK(hit.reference == handle);
    CHECK(near(hit.distance, 10.0f));

    // Just outside the 4 x 2 picture, and from behind it pointing away
    CHECK(!pickReference(scene, rayAlongMinusZ(2.1f, 0.0f)).hit);
    CHECK(!pickReference(scene, rayAlongMinusZ(0.0f, 1.1f)).hit);
    CHECK(!pickReference(scene, { Vec3(0.0f, 0.0f, 10.0f), Vec3(0.0f, 0.0f, 1.0f) }).hit);

    // The back is clickable too
    CHECK(pickReference(scene, { Vec3(0.0f, 0.0f, -3.0f), Vec3(0.0f, 0.0f, 1.0f) }).hit);

    // Turned and moved: still found where it is
    ReferenceImage& image = scene.references.get(handle);
    image.position = Vec3(5.0f, 0.0f, 0.0f);
    image.rotation = Vec3(0.0f, 1.5707964f, 0.0f);
    image.size = 4.0f;
    hit = pickReference(scene, { Vec3(20.0f, 1.9f, 3.9f), Vec3(-1.0f, 0.0f, 0.0f) });
    CHECK(hit.hit);
    CHECK(near(hit.distance, 15.0f));
    CHECK(!pickReference(scene, rayAlongMinusZ(0.0f, 0.0f)).hit);
}

TEST_CASE(pick_reference_skips_locked_and_hidden_and_ranks_by_depth) {
    Scene scene;
    ReferenceImage near = wideImage();
    near.position.z = 2.0f;
    const ReferenceHandle front = scene.references.add(near);
    const ReferenceHandle back = scene.references.add(wideImage());

    CHECK(pickReference(scene, rayAlongMinusZ(0.0f, 0.0f)).reference == front);

    scene.references.get(front).locked = true;
    CHECK(pickReference(scene, rayAlongMinusZ(0.0f, 0.0f)).reference == back);
    scene.references.get(front).locked = false;
    scene.references.get(front).visible = false;
    CHECK(pickReference(scene, rayAlongMinusZ(0.0f, 0.0f)).reference == back);
    scene.references.get(front).visible = true;

    // In front of everything wins over nearer images; behind everything loses to farther ones
    scene.references.get(back).depth = ReferenceDepth::InFront;
    CHECK(pickReference(scene, rayAlongMinusZ(0.0f, 0.0f)).reference == back);
    scene.references.get(back).depth = ReferenceDepth::InScene;
    scene.references.get(front).depth = ReferenceDepth::Behind;
    const ReferenceHit hit = pickReference(scene, rayAlongMinusZ(0.0f, 0.0f));
    CHECK(hit.reference == back);
    CHECK(hit.depth == ReferenceDepth::InScene);

    scene.references.get(back).locked = true;
    CHECK(pickReference(scene, rayAlongMinusZ(0.0f, 0.0f)).depth == ReferenceDepth::Behind);
}

TEST_CASE(reference_images_undo_and_share_their_picture) {
    Scene scene;
    History history;

    history.begin(scene);
    const ReferenceHandle handle = scene.references.add(wideImage());
    history.commit();

    const ReferencePicture* shared = scene.references.get(handle).picture.get();

    history.begin(scene);
    scene.references.get(handle).opacity = 0.25f;
    history.commit();

    CHECK(history.undo(scene));
    CHECK(near(scene.references.get(handle).opacity, 1.0f));
    // Undo copies the image, not its picture
    CHECK(scene.references.get(handle).picture.get() == shared);

    CHECK(history.undo(scene));
    CHECK(scene.references.count() == 0);
    CHECK(history.redo(scene));
    CHECK(scene.references.count() == 1);
}

TEST_CASE(project_saves_reference_images) {
    Scene scene;
    ReferenceImage image = wideImage();
    image.picture = picture();
    image.position = Vec3(1.0f, 2.0f, -3.0f);
    image.rotation = Vec3(0.1f, -0.2f, 0.3f);
    image.size = 3.5f;
    image.opacity = 0.4f;
    image.depth = ReferenceDepth::Behind;
    image.locked = true;
    image.visible = false;
    scene.references.add(image);

    ReferenceImage second = wideImage();
    second.name = "Side";
    second.picture = picture();
    scene.references.add(second);

    ProjectFile::View view;
    const std::vector<u8> bytes = ProjectFile::write(scene, view);

    Scene loaded;
    ProjectFile::View loadedView;
    std::string error;
    CHECK(ProjectFile::read(bytes, loaded, loadedView, error));
    CHECK(loaded.references.count() == 2);

    const ReferenceImage& first = loaded.references.get(loaded.references.handleAt(0));
    CHECK(first.name == "Front");
    CHECK(nearVec3(first.position, image.position));
    CHECK(nearVec3(first.rotation, image.rotation));
    CHECK(first.size == 3.5f);
    CHECK(first.opacity == 0.4f);
    CHECK(first.depth == ReferenceDepth::Behind);
    CHECK(first.locked);
    CHECK(!first.visible);
    CHECK(first.picture->fileName == "rgba8.png");
    CHECK(first.picture->width == 13 && first.picture->height == 7);
    CHECK(first.picture->png == image.picture->png);
    CHECK(loaded.references.get(loaded.references.handleAt(1)).name == "Side");

    CHECK(ProjectFile::describe(bytes).find("REFI") != std::string::npos);
}

TEST_CASE(project_refuses_a_reference_that_isnt_a_png) {
    Scene scene;
    ReferenceImage image = wideImage();
    auto broken = std::make_shared<ReferencePicture>(*picture());
    broken->png[1] = 'X';
    image.picture = broken;
    scene.references.add(image);

    Scene loaded;
    ProjectFile::View view;
    std::string error;
    CHECK(!ProjectFile::read(ProjectFile::write(scene, view), loaded, view, error));
    CHECK(error.find("damaged") != std::string::npos);
}
