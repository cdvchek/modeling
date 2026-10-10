#include "test.hpp"
#include "mesh_helpers.hpp"
#include "scene/objects/object_collection.hpp"

#include <cmath>

namespace {
    bool near(f32 a, f32 b, f32 tolerance = 1e-4f) {
        return std::fabs(a - b) <= tolerance;
    }

    bool same(const Vec3& a, const Vec3& b, f32 tolerance = 1e-4f) {
        return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance) && near(a.z, b.z, tolerance);
    }

    // The same placement: both turn and scale test points to the same places
    bool sameTransform(const Transform& a, const Transform& b) {
        const ObjectSpace sa(a), sb(b);
        for (const Vec3& p : { Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0.3f, -0.5f, 0.8f) }) {
            if (!same(sa.pointToWorld(p), sb.pointToWorld(p))) return false;
        }
        return true;
    }

    Transform transform(Vec3 position, Vec3 rotation, Vec3 scale) {
        Transform t;
        t.position = position;
        t.rotation = rotation;
        t.scale = scale;
        return t;
    }

    // Columns of a matrix with no skew are at right angles to each other
    bool noSkew(const Mat4& m) {
        const Vec3 x(m[0][0], m[0][1], m[0][2]);
        const Vec3 y(m[1][0], m[1][1], m[1][2]);
        const Vec3 z(m[2][0], m[2][1], m[2][2]);
        const f32 scale = x.length() * y.length() + 1e-6f;
        return std::fabs(Vec3::dot(x, y)) < 1e-4f * scale && std::fabs(Vec3::dot(y, z)) < 1e-4f * scale && std::fabs(Vec3::dot(x, z)) < 1e-4f * scale;
    }
}

TEST_CASE(parenting_combine_places_scales_and_turns_without_skew) {
    // A child one unit along X of a parent at (5, 0, 0), stretched 2x along X, turned 90 degrees about Y
    const Transform parent = transform(Vec3(5, 0, 0), Vec3(0, 1.5707964f, 0), Vec3(2, 1, 1));
    const Transform child = transform(Vec3(1, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1));
    const Transform world = combineTransforms(parent, child);

    // The position goes where it belongs: scaled to 2 along the parent's X, which now points along -Z
    CHECK(same(world.position, Vec3(5, 0, -2)));
    CHECK(same(world.rotation, Vec3(0, 1.5707964f, 0)));
    // The parent's scale lands on the child's own axes
    CHECK(same(world.scale, Vec3(2, 1, 1)));

    // A child turned inside an unevenly stretched parent still has no skew
    const Transform turned = transform(Vec3(0.5f, 1, 0), Vec3(0.4f, 0.9f, -0.3f), Vec3(1, 0.5f, 3));
    const Transform stretched = transform(Vec3(1, 2, 3), Vec3(0.2f, -0.7f, 0.5f), Vec3(3, 1, 0.5f));
    CHECK(noSkew(combineTransforms(stretched, turned).getMatrix()));
}

TEST_CASE(parenting_relative_is_the_exact_reverse) {
    const Transform parent = transform(Vec3(1, 2, 3), Vec3(0.2f, -0.7f, 0.5f), Vec3(3, 1, 0.5f));
    const Transform local = transform(Vec3(0.5f, 1, -2), Vec3(0.4f, 0.9f, -0.3f), Vec3(1, 0.5f, 3));

    const Transform back = relativeTransform(parent, combineTransforms(parent, local));
    CHECK(same(back.position, local.position));
    CHECK(sameTransform(transform(Vec3(0), back.rotation, Vec3(1)), transform(Vec3(0), local.rotation, Vec3(1))));
    CHECK(same(back.scale, local.scale));
}

TEST_CASE(parenting_keeps_objects_where_they_are) {
    ObjectCollection objects;
    const ObjectHandle car = objects.add("Car", PresetMesh::Cube);
    const ObjectHandle wheel = objects.add("Wheel", PresetMesh::Cylinder);
    objects.get(car).transform = transform(Vec3(4, 0, 0), Vec3(0, 0.6f, 0), Vec3(2, 1, 3));
    objects.get(wheel).transform = transform(Vec3(5, -0.5f, 1), Vec3(0, 0, 1.5707964f), Vec3(0.5f, 0.5f, 0.5f));

    const Transform before = objects.worldTransform(wheel);
    CHECK(objects.setParent(wheel, car));
    CHECK(objects.parentOf(wheel) == car);
    CHECK(sameTransform(objects.worldTransform(wheel), before));

    // Moving the parent carries the child
    objects.get(car).transform.position += Vec3(0, 3, 0);
    CHECK(same(objects.worldTransform(wheel).position, before.position + Vec3(0, 3, 0)));

    // Clearing the parent keeps it where it is now
    const Transform carried = objects.worldTransform(wheel);
    CHECK(objects.setParent(wheel, INVALID_OBJECT));
    CHECK(objects.parentOf(wheel).isNull());
    CHECK(sameTransform(objects.get(wheel).transform, carried));
}

TEST_CASE(parenting_refuses_loops_and_lists_the_tree) {
    ObjectCollection objects;
    const ObjectHandle a = objects.add("A", PresetMesh::Cube);
    const ObjectHandle b = objects.add("B", PresetMesh::Cube);
    const ObjectHandle c = objects.add("C", PresetMesh::Cube);
    const ObjectHandle d = objects.add("D", PresetMesh::Cube);

    CHECK(objects.setParent(b, a));
    CHECK(objects.setParent(c, b));
    CHECK(!objects.setParent(a, c));   // a would be its own grandchild's child
    CHECK(!objects.setParent(a, a));
    CHECK(objects.parentOf(a).isNull());

    CHECK(objects.isAncestor(a, c) && !objects.isAncestor(c, a));
    CHECK(objects.topLevelOf(c) == a && objects.topLevelOf(d) == d);
    CHECK(objects.childrenOf(a).size() == 1 && objects.childrenOf(a)[0] == b);

    // Parents before children, depth counted from the top
    const std::vector<HierarchyEntry> rows = objects.hierarchy();
    CHECK(rows.size() == 4);
    CHECK(rows[0].handle == a && rows[0].depth == 0);
    CHECK(rows[1].handle == b && rows[1].depth == 1);
    CHECK(rows[2].handle == c && rows[2].depth == 2);
    CHECK(rows[3].handle == d && rows[3].depth == 0);
}

TEST_CASE(parenting_removing_a_parent_moves_children_up) {
    ObjectCollection objects;
    const ObjectHandle top = objects.add("Top", PresetMesh::Cube);
    const ObjectHandle middle = objects.add("Middle", PresetMesh::Cube);
    const ObjectHandle bottom = objects.add("Bottom", PresetMesh::Cube);
    objects.get(top).transform = transform(Vec3(1, 2, 3), Vec3(0.3f, 0, 0), Vec3(2, 2, 2));
    objects.get(middle).transform.position = Vec3(4, 0, 0);
    objects.get(bottom).transform.position = Vec3(0, 5, 0);
    objects.setParent(middle, top);
    objects.setParent(bottom, middle);

    const Transform before = objects.worldTransform(bottom);
    objects.remove(middle);
    CHECK(objects.parentOf(bottom) == top);
    CHECK(sameTransform(objects.worldTransform(bottom), before));

    // With no grandparent, it goes to the top level
    objects.remove(top);
    CHECK(objects.parentOf(bottom).isNull());
    CHECK(sameTransform(objects.worldTransform(bottom), before));
}

TEST_CASE(parenting_world_position_keeps_rotation_and_scale) {
    ObjectCollection objects;
    const ObjectHandle parent = objects.add("Parent", PresetMesh::Cube);
    const ObjectHandle child = objects.add("Child", PresetMesh::Cube);
    objects.get(parent).transform = transform(Vec3(1, 0, 0), Vec3(0, 0.8f, 0), Vec3(2, 1, 1));
    objects.get(child).transform = transform(Vec3(0, 1, 0), Vec3(0.2f, 0.4f, 0.6f), Vec3(1, 3, 1));
    objects.get(child).parent = parent;

    const Transform local = objects.get(child).transform;
    objects.setWorldPosition(child, Vec3(7, 8, 9));
    CHECK(same(objects.worldTransform(child).position, Vec3(7, 8, 9)));
    CHECK(objects.get(child).transform.rotation.x == local.rotation.x && objects.get(child).transform.scale.y == local.scale.y);
}
