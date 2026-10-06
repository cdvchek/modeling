#include "test.hpp"
#include "core/math/screen_drag.hpp"

namespace {
    bool near(f32 a, f32 b) { return std::abs(a - b) < 1e-4f; }
}

TEST_CASE(screen_angle_is_counterclockwise_on_screen) {
    const Vec2 pivot(100.0f, 100.0f);

    // Screen y points down, so up on screen is a quarter turn counterclockwise from right
    CHECK(near(screenAngle(pivot, Vec2(150.0f, 100.0f)), 0.0f));
    CHECK(near(screenAngle(pivot, Vec2(100.0f, 50.0f)), Math::HALF_PI));
    CHECK(near(screenAngle(pivot, Vec2(100.0f, 150.0f)), -Math::HALF_PI));
}

TEST_CASE(wrap_angle_takes_the_short_way_across_the_seam) {
    CHECK(near(wrapAngle(0.5f), 0.5f));
    CHECK(near(wrapAngle(Math::TWO_PI - 0.1f), -0.1f));
    CHECK(near(wrapAngle(-Math::TWO_PI + 0.1f), 0.1f));
}

TEST_CASE(summed_wrapped_steps_count_full_turns) {
    // Circling the pivot once counterclockwise in small steps adds up to one full turn
    const Vec2 pivot(0.0f, 0.0f);
    f32 total = 0.0f;
    f32 last = screenAngle(pivot, Vec2(10.0f, 0.0f));

    for (u32 i = 1; i <= 36; ++i) {
        const f32 a = Math::TWO_PI * static_cast<f32>(i) / 36.0f;
        const f32 angle = screenAngle(pivot, Vec2(10.0f * std::cos(a), -10.0f * std::sin(a)));
        total += wrapAngle(angle - last);
        last = angle;
    }

    CHECK(near(total, Math::TWO_PI));
}
