#include "test.hpp"
#include "core/math/projection.hpp"

#include <cmath>

namespace {
    bool near(f32 a, f32 b) { return std::abs(a - b) < 0.01f; }

    Mat4 lookingDownNegativeZ() {
        const Mat4 view = Mat4::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f));
        return Mat4::perspective(1.0471975f, 2.0f, 0.1f, 100.0f) * view;
    }
}

TEST_CASE(projection_center_and_axes) {
    const Mat4 viewProjection = lookingDownNegativeZ();
    Vec2 screen;

    CHECK(projectToScreen(viewProjection, Vec3(0.0f), 200.0f, 100.0f, screen));
    CHECK(near(screen.x, 100.0f));
    CHECK(near(screen.y, 50.0f));

    // +X lands right of center, +Y lands above center (smaller y)
    CHECK(projectToScreen(viewProjection, Vec3(1.0f, 0.0f, 0.0f), 200.0f, 100.0f, screen));
    CHECK(screen.x > 100.0f);
    CHECK(projectToScreen(viewProjection, Vec3(0.0f, 1.0f, 0.0f), 200.0f, 100.0f, screen));
    CHECK(screen.y < 50.0f);
}

TEST_CASE(projection_rejects_points_behind_camera) {
    Vec2 screen;
    CHECK(!projectToScreen(lookingDownNegativeZ(), Vec3(0.0f, 0.0f, 10.0f), 200.0f, 100.0f, screen));
}
