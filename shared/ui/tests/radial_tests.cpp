#include "test.hpp"
#include "ui/radial_layout.hpp"
#include "ui/ui_draw_list.hpp"

#include <cmath>

TEST_CASE(radial_eight_slices_start_up_and_go_clockwise) {
    // Screen y points down, so up is negative y
    CHECK(RadialLayout::sliceAt(Vec2(0.0f, -100.0f), 20.0f, 8) == 0);
    CHECK(RadialLayout::sliceAt(Vec2(70.0f, -70.0f), 20.0f, 8) == 1);
    CHECK(RadialLayout::sliceAt(Vec2(100.0f, 0.0f), 20.0f, 8) == 2);
    CHECK(RadialLayout::sliceAt(Vec2(0.0f, 100.0f), 20.0f, 8) == 4);
    CHECK(RadialLayout::sliceAt(Vec2(-100.0f, 0.0f), 20.0f, 8) == 6);
    CHECK(RadialLayout::sliceAt(Vec2(-70.0f, -70.0f), 20.0f, 8) == 7);
}

TEST_CASE(radial_four_slices_are_up_right_down_left) {
    CHECK(RadialLayout::sliceAt(Vec2(0.0f, -100.0f), 20.0f, 4) == 0);
    CHECK(RadialLayout::sliceAt(Vec2(100.0f, 0.0f), 20.0f, 4) == 1);
    CHECK(RadialLayout::sliceAt(Vec2(0.0f, 100.0f), 20.0f, 4) == 2);
    CHECK(RadialLayout::sliceAt(Vec2(-100.0f, 0.0f), 20.0f, 4) == 3);

    // Each slice is a quarter: just short of the diagonal still picks up
    CHECK(RadialLayout::sliceAt(Vec2(40.0f, -60.0f), 20.0f, 4) == 0);
    CHECK(RadialLayout::sliceAt(Vec2(60.0f, -40.0f), 20.0f, 4) == 1);
}

TEST_CASE(radial_odd_counts_mirror_left_and_right_only) {
    // Five slices: the first is centered up, the rest mirror across the vertical
    const Vec2 first = RadialLayout::sliceDirection(0, 5);
    CHECK(std::abs(first.x) < 1e-5f && first.y < 0.0f);

    const Vec2 second = RadialLayout::sliceDirection(1, 5);
    const Vec2 last = RadialLayout::sliceDirection(4, 5);
    CHECK(std::abs(second.x + last.x) < 1e-5f);
    CHECK(std::abs(second.y - last.y) < 1e-5f);

    // Nothing points straight down
    CHECK(RadialLayout::sliceAt(Vec2(0.0f, 100.0f), 20.0f, 5) != RadialLayout::sliceAt(Vec2(1.0f, 100.0f), 20.0f, 5)
          || RadialLayout::sliceAt(Vec2(-1.0f, 100.0f), 20.0f, 5) != RadialLayout::sliceAt(Vec2(1.0f, 100.0f), 20.0f, 5));
}

TEST_CASE(radial_dead_zone_picks_nothing) {
    CHECK(RadialLayout::sliceAt(Vec2(10.0f, 5.0f), 20.0f, 8) == -1);
    CHECK(RadialLayout::sliceAt(Vec2(0.0f, -100.0f), 20.0f, 0) == -1);
}

TEST_CASE(radial_slice_direction_matches_picking) {
    for (u32 count : { 2u, 3u, 4u, 5u, 8u }) {
        for (u32 slice = 0; slice < count; ++slice) {
            CHECK(RadialLayout::sliceAt(RadialLayout::sliceDirection(slice, count) * 50.0f, 20.0f, count) == static_cast<i32>(slice));
        }
    }
}

TEST_CASE(ui_ring_slice_is_one_quad_around_the_center) {
    UIDrawList list;
    list.ringSlice(Vec2(100.0f, 100.0f), 20.0f, 50.0f, 0.0f, 0.39f, 4.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

    CHECK(list.getVertices().size() == 4);
    CHECK(list.getIndices().size() == 6);

    const UIVertex& corner = list.getVertices()[0];
    CHECK(corner.mode == UIDrawList::MODE_RING_SLICE);
    CHECK(corner.halfSize.x == 20.0f);
    CHECK(corner.halfSize.y == 50.0f);
    CHECK(std::abs(corner.position.x - 49.0f) < 1e-4f);
    CHECK(std::abs(corner.position.y - 49.0f) < 1e-4f);
}

TEST_CASE(ui_ring_slice_local_axes_follow_its_angle) {
    // A slice pointing up: the corner up and to the right is ahead of the slice's middle and to its right
    UIDrawList list;
    list.ringSlice(Vec2(0.0f, 0.0f), 20.0f, 50.0f, 1.5707964f, 0.39f, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

    const UIVertex& upRight = list.getVertices()[1];
    CHECK(upRight.position.x > 0.0f && upRight.position.y < 0.0f);
    CHECK(std::abs(upRight.local.x - 51.0f) < 1e-3f);
    CHECK(std::abs(std::abs(upRight.local.y) - 51.0f) < 1e-3f);
}
