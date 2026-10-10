#include "test.hpp"
#include "core/time/frame_timer.hpp"

#include <cmath>

TEST_CASE(frame_timer_starts_at_zero) {
    FrameTimer timer;
    CHECK(timer.getFps() == 0.0f);

    timer.tick(0.0);
    timer.tick(0.1);
    CHECK(timer.getFps() == 0.0f);
}

TEST_CASE(frame_timer_averages_over_window) {
    FrameTimer timer;

    // 60 frames per second for one window
    for (u32 frame = 0; frame <= 30; ++frame) timer.tick(frame / 60.0);
    CHECK(std::abs(timer.getFps() - 60.0f) < 0.01f);

    // Holds the value until the next window finishes
    timer.tick(0.5 + 1.0 / 30.0);
    CHECK(std::abs(timer.getFps() - 60.0f) < 0.01f);
}

TEST_CASE(frame_timer_follows_slower_frames) {
    FrameTimer timer;
    for (u32 frame = 0; frame <= 30; ++frame) timer.tick(frame / 60.0);

    // 10 frames per second for the next window
    for (u32 frame = 1; frame <= 5; ++frame) timer.tick(0.5 + frame / 10.0);
    CHECK(std::abs(timer.getFps() - 10.0f) < 0.01f);
}
