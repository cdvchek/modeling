#include "test.hpp"
#include "core/input/context_manager.hpp"
#include "core/math/vec4.hpp"
#include "scene/selection/selection.hpp"
#include "scene/transform.hpp"

#include <cmath>

namespace {
    bool near(Vec3 a, Vec3 b) {
        return std::abs(a.x - b.x) < 1e-4f && std::abs(a.y - b.y) < 1e-4f && std::abs(a.z - b.z) < 1e-4f;
    }

    Vec3 apply(const Transform& transform, Vec3 point) {
        const Vec4 result = transform.getMatrix() * Vec4(point.x, point.y, point.z, 1.0f);
        return Vec3(result.x, result.y, result.z);
    }

    // Rodrigues' rotation formula, for checking against
    Vec3 rotateAround(Vec3 v, Vec3 axis, f32 angle) {
        const Vec3 k = axis.normalized();
        return v * std::cos(angle) + Vec3::cross(k, v) * std::sin(angle) + k * Vec3::dot(k, v) * (1.0f - std::cos(angle));
    }

    // The new Euler angles must turn every point the same way as rotating the old result around the axis
    bool rotatesLikeAxisAngle(Vec3 euler, Vec3 axis, f32 angle) {
        Transform before;
        before.rotation = euler;

        Transform after;
        after.rotation = rotateEuler(euler, axis, angle);

        for (Vec3 point : { Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.3f, -0.7f, 0.5f) }) {
            if (!near(apply(after, point), rotateAround(apply(before, point), axis, angle))) return false;
        }
        return true;
    }
}

TEST_CASE(object_selection_select_deselect_and_clear) {
    ObjectCollection objects;
    const ObjectHandle a = objects.add("A", PresetMesh::Cube);
    const ObjectHandle b = objects.add("B", PresetMesh::Cube);

    Selection selection;
    selection.selectObject(a);
    selection.selectObject(b);
    selection.selectObject(a);
    CHECK(selection.getObjects().size() == 2);
    CHECK(selection.hasObject(a) && selection.hasObject(b));

    selection.deselectObject(a);
    CHECK(!selection.hasObject(a));
    CHECK(selection.hasObjects());

    // Changing the active object keeps the object selection; clear() drops it
    selection.setActiveObject(b);
    CHECK(selection.hasObject(b));
    selection.clear();
    CHECK(!selection.hasObjects());
    CHECK(selection.getActiveObject() == b);
}

TEST_CASE(object_mode_is_one_selection_context) {
    ContextManager contexts;
    contexts.setSelectionContext(InputContext_SelectionObject);
    CHECK(contexts.getSelectionContext() == InputContext_SelectionObject);
    CHECK(contexts.isActive(InputContext_AnySelection));
    CHECK(!contexts.isActive(InputContext_EditModes));

    // A tool replaces the selection context, then confirming brings object mode back
    contexts.setContext(InputContext_Grab);
    CHECK(!contexts.isActive(InputContext_AnySelection));
    contexts.setContext(contexts.getSelectionContext());
    CHECK(contexts.isActive(InputContext_SelectionObject));

    contexts.setSelectionContext(InputContext_SelectionFace);
    CHECK(!contexts.isActive(InputContext_SelectionObject));
}

TEST_CASE(rotate_euler_matches_axis_angle) {
    CHECK(rotatesLikeAxisAngle(Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f), 0.7f));
    CHECK(rotatesLikeAxisAngle(Vec3(0.3f, -0.4f, 1.1f), Vec3(1.0f, 0.0f, 0.0f), 0.5f));
    CHECK(rotatesLikeAxisAngle(Vec3(0.3f, -0.4f, 1.1f), Vec3(0.2f, 0.9f, -0.4f), -2.3f));
    CHECK(rotatesLikeAxisAngle(Vec3(-1.2f, 0.6f, -0.2f), Vec3(0.0f, 0.0f, 1.0f), 3.0f));
}

TEST_CASE(rotate_euler_survives_looking_straight_along_y) {
    // A quarter turn about Y lands right on the gimbal-lock angle
    CHECK(rotatesLikeAxisAngle(Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f), 1.5707964f));
    CHECK(rotatesLikeAxisAngle(Vec3(0.4f, 0.0f, 0.2f), Vec3(0.0f, 1.0f, 0.0f), -1.5707964f));
}

TEST_CASE(object_space_round_trips_points_and_directions) {
    Transform transform;
    transform.position = Vec3(1.0f, -2.0f, 0.5f);
    transform.rotation = Vec3(0.4f, 1.1f, -0.3f);
    transform.scale = Vec3(2.0f, 0.5f, 1.5f);
    const ObjectSpace space(transform);

    const Vec3 point(0.3f, 0.7f, -0.2f);
    CHECK(near(space.pointToWorld(point), apply(transform, point)));
    CHECK(near(space.pointToLocal(space.pointToWorld(point)), point));

    // A world move converted to mesh space moves the world point by exactly that much
    const Vec3 move(0.25f, -0.5f, 1.0f);
    CHECK(near(space.pointToWorld(point + space.directionToLocal(move)), space.pointToWorld(point) + move));
}

TEST_CASE(object_space_world_axis_on_a_rotated_object) {
    // A quarter turn about Z: world +X is the mesh's -Y
    Transform transform;
    transform.rotation = Vec3(0.0f, 0.0f, 1.5707964f);
    const ObjectSpace space(transform);

    CHECK(near(space.directionToLocal(Vec3(1.0f, 0.0f, 0.0f)), Vec3(0.0f, -1.0f, 0.0f)));
}
